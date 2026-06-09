/*
 * @author: e.he
 * @Date: 2024-07-10 16:29:48
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-10-09 16:31:10
 * @Description: 
 * 待机状态->启动成功:qhxyj_devSta.isAlarm:0不变
 *                     qhxyj_devSta.isRun:0->1
 *                 qhxyj_devSta.isAutoRun:0->1
 *                   qhxyj_devSta.isReady:0->1 
 * 
 * 待机状态->启动失败: qhxyj_devSta.isAlarm:0不变
 *                     qhxyj_devSta.isRun:0不变
 *                 qhxyj_devSta.isAutoRun:0不变
 *                   qhxyj_devSta.isReady:0不变 
 * 
 * 正常运行->发生故障，机器停止运行:  qhxyj_devSta.isAlarm:0->1
 *                                    qhxyj_devSta.isRun:1->0
 *                                qhxyj_devSta.isAutoRun:1->0
 *                                  qhxyj_devSta.isReady:1->0
 * 
 */
#include "lyy_xyj.h"
#include "lyy_device.h"
#include "lyy_tjc.h"

#include "APP_Param.h"
#include "APP_Pluse.h"
#include "APP_Detect.h"
#include "APP_DrillingMode.h"

#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_WIFI.h"
#include "HAL_Uart.h"
#include "HAL_System.h"
#include "HAL_Flash.h"
#include "HAL_Sem.h"
#include "thing_event.h"

#include "thing_iot.h"
#include "thing_mqtt.h"
//#include "sys_packet.h"
//#include "os_inter.h"
#include "lyy_para.h"
//#include "bsp_osInter.h"

static uint8_t flagLoopCheckSta = 1;//循环获取状态启动标志
static uint8_t flagFristGetSta = 0;//首次获取到主板状态标志
static uint8_t flagReportDevStatus = 0;//满足上报主板状态标志
static int mqtt_status_old = 0;//记录mqtt连接状态
static int flagfactoryParmGet = 0;//记录主板参数获取标记 

static SEM_TYPE      qixyj_checkStaus_sem;
static SEM_TYPE      qixyj_selectProgram_sem;
static SEM_TYPE      qhxyj_sendCoin_sem;
static SEM_TYPE      qixyj_start_sem;
static SEM_TYPE      qixyj_getWashRunData_sem;
static SEM_TYPE      qixyj_setProgram_sem;

TYPE_QHXYJ_DEV_STA qhxyj_devSta={0};//状态信息
TYPE_REPORT_QHXYJ_STA_INFO qhxyj_washRundata={0};
TYPE_REPORT_QHXYJ_ACCOUNT_INFO  qhxyj_account = {0};
QHXYJ_FACTORY_PAY_SETTING qhxyj_pay_setting = {0};
static uint32_t alarmRecord_old = 0;
uint8_t qhxyjAtuoRunSta_old = 0;//对应qhxyj_devSta.isAutoRun
uint16_t qhxyjCoinCnt_old = 0;//对应qhxyj_account.coin_all;
uint8_t qhxyjfactoryParamId = 0; //控制读取参数序号
uint8_t qhxyjstepParamId = 0; //控制读取步骤参数序号

#define TIME_INTERRUPT_MS      (50U)  //定时器周期
#define PERIOD_FOR_CHECKSTA    (3000U) //获取设备状态的周期
static int timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;
/*******************************************************************************
*函数名称：uint16_t ModBusCRC16_long(uint8_t *cmd, unsigned long len)
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：CRC16 ModBus 协议
*******************************************************************************/
uint16_t ModBusCRC16_long(uint8_t *cmd, unsigned long len)
{
	unsigned long i;
	uint8_t j;
	uint16_t tmp, CRC16;
	// CRC16的算法原理：
	// 1. 根据CRC16的标准选择初值CRCIn的值；
	CRC16 = 0xFFFF; // CRC寄存器初始值
	for (i = 0; i < len; i++)
	{
		CRC16 ^= cmd[i];
		for (j = 0; j < 8; j++)
		{ // 2. 将数据的第一个字节与CRCIn高8位异或；
			tmp = (uint16_t)(CRC16 & 0x0001);
			CRC16 >>= 1;
			// 3. 判断最高位，若该位为 0 左移一位，若为 1 左移一位再与多项式Hex码异或；
			if (tmp == 1)
			{
				CRC16 ^= 0xA001; //异或多项式
			}
		}
	// 4. 重复3直至8位全部移位计算结束；
	}
	// 5. 重复将所有输入数据操作完成以上步骤，所得16位数即16位CRC校验码。
	return CRC16;
}

/*报警信息打印 */
static void device_alarm_info_print(void)
{
    if( qhxyj_devSta.isAlarm )
    {
        LYY_LOGD("!!!!!!!!!Alarm!!!!!!!!! qhxyj_devSta.alarmRecord:%X",qhxyj_devSta.alarmRecord);
        if(qhxyj_devSta.alarmRecord&Alarm_HMI_communication_station_not_connect)
            LYY_LOGD("Alarm_HMI_communication_station_not_connect");//:通信失败
        if(qhxyj_devSta.alarmRecord&Alarm_emergency)
            LYY_LOGD("Alarm_emergency");//急停报警
        if(qhxyj_devSta.alarmRecord&Alarm_converter_fault)
            LYY_LOGD("Alarm_converter_fault");//变频器故障
        if(qhxyj_devSta.alarmRecord&Alarm_machine_overshak)
            LYY_LOGD("Alarm_machine_overshak");//严重超震
        if(qhxyj_devSta.alarmRecord&Alarm_door_station_abnormal)
            LYY_LOGD("Alarm_door_station_abnormal");//门状态异常
        if(qhxyj_devSta.alarmRecord&Alarm_door_bolt_input_fault)
            LYY_LOGD("Alarm_door_bolt_input_fault");//门销输入错误
        if(qhxyj_devSta.alarmRecord&Alarm_add_water_overtime)
            LYY_LOGD("Alarm_add_water_overtime");//加水超时
        if(qhxyj_devSta.alarmRecord&Alarm_heat_overtime)
            LYY_LOGD("Alarm_heat_overtime");//加热超时
        if(qhxyj_devSta.alarmRecord&Alarm_drain_water_overtime)
            LYY_LOGD("Alarm_drain_water_overtime");//排水超时
        if(qhxyj_devSta.alarmRecord&Alarm_exceed_temperature)
            LYY_LOGD("Alarm_exceed_temperature");//加热超温
        if(qhxyj_devSta.alarmRecord&Alarm_water_level_parameter_not_calbrication)
            LYY_LOGD("Alarm_water_level_parameter_not_calbrication");//液位参数未校准
        if(qhxyj_devSta.alarmRecord&Alarm_temperature_sensor_open_circuit)
            LYY_LOGD("Alarm_temperature_sensor_open_circuit");//温度检测器开路
        if(qhxyj_devSta.alarmRecord&Alarm_temperature_sensor_short_circuit)
            LYY_LOGD("Alarm_temperature_sensor_short_circuit");//温度检测器短路
    }
}
/*报警信息上送 */
static void device_alarm_info_up_raw(void)// done
{
    if( qhxyj_devSta.isAlarm )
    {
        LYY_LOGD("!!!!!!!!!Alarm!!!!!!!!! qhxyj_devSta.alarmRecord:%X",qhxyj_devSta.alarmRecord);
        if(qhxyj_devSta.alarmRecord&Alarm_HMI_communication_station_not_connect)
        {
            LYY_LOGD("Alarm_HMI_communication_station_not_connect");//:通信失败
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_communication_station_not_connect);
        }   
        if(qhxyj_devSta.alarmRecord&Alarm_emergency)
        {
            LYY_LOGD("Alarm_emergency");//急停报警
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_emergency);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_converter_fault)
        {
            LYY_LOGD("Alarm_converter_fault");//变频器故障
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_converter_fault);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_machine_overshak)
        {
            LYY_LOGD("Alarm_machine_overshak");//严重超震
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_machine_overshak);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_door_station_abnormal)
        {
            LYY_LOGD("Alarm_door_station_abnormal");//门状态异常
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_door_station_abnormal);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_door_bolt_input_fault)
        {
            LYY_LOGD("Alarm_door_bolt_input_fault");//门销输入错误
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_door_bolt_input_fault);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_add_water_overtime)
        {
            LYY_LOGD("Alarm_add_water_overtime");//加水超时
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_add_water_overtime);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_heat_overtime)
        {
            LYY_LOGD("Alarm_heat_overtime");//加热超时
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_heat_overtime);
        }

            
        if(qhxyj_devSta.alarmRecord&Alarm_drain_water_overtime)
        {
            LYY_LOGD("Alarm_drain_water_overtime");//排水超时
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_drain_water_overtime);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_exceed_temperature)
        {
            LYY_LOGD("Alarm_exceed_temperature");//加热超温
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_exceed_temperature);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_water_level_parameter_not_calbrication)
        {
            LYY_LOGD("Alarm_water_level_parameter_not_calbrication");//液位参数未校准
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_water_level_parameter_not_calbrication);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_temperature_sensor_open_circuit)
        {
            LYY_LOGD("Alarm_temperature_sensor_open_circuit");//温度检测器开路
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_temperature_sensor_open_circuit);
        }
            
        if(qhxyj_devSta.alarmRecord&Alarm_temperature_sensor_short_circuit)
        {
            LYY_LOGD("Alarm_temperature_sensor_short_circuit");//温度检测器短路  
            qhxyj_ctlb_post_up_raw_err(ErrCode_XYJ_temperature_sensor_short_circuit);
        }
            
    }
}
/**
 * @brief 
 * 乔禾洗衣机位状态解析
 * @param pay_id 
 * @param sub_id 
 * @param id 
 * @return
 */
void qhxyj_prase_bitCheckStutus(uint8_t *package, uint16_t len)
{
    (void)len;

    LYY_LOGD("bit status>>>>>>>>>>>>>>>>BYTE7:0x%X",package[2+7]);
    qhxyj_devSta.isRun = ((package[2+7]&0x06)>>1);
    qhxyj_devSta.isAlarm = (package[2+7]&(1<<3)) ? 1:0;
    qhxyj_devSta.isReady = (package[2+7]&(1<<4)) ? 1:0;
    LYY_LOGD("    qhxyj_devSta.isRun :%d",qhxyj_devSta.isRun);
    LYY_LOGD("  qhxyj_devSta.isAlarm :%d",qhxyj_devSta.isAlarm);
    LYY_LOGD("  qhxyj_devSta.isReady :%d",qhxyj_devSta.isReady);
    LYY_LOGD("bit status>>>>>>>>>>>>>>>>BYTE14:0x%X",package[2+14]);
    qhxyj_devSta.isDoorOk = (package[2+14]&(1<<5)) ? 1:0;
    LYY_LOGD(" qhxyj_devSta.isDoorOk :%d",qhxyj_devSta.isDoorOk);
    LYY_LOGD("bit status>>>>>>>>>>>>>>>>BYTE15:0x%X",package[2+15]);
    qhxyj_devSta.isAutoRun = (package[2+15]&(1<<0)) ? 1:0;
    LYY_LOGD("qhxyj_devSta.isAutoRun :%d",qhxyj_devSta.isAutoRun);

    uint32_t alarmRecordTemp = qhxyj_devSta.alarmRecord;
    if(qhxyj_devSta.isAlarm==0)
    {/*无故障报警*/
        alarmRecordTemp = 0;
    }
    else
    {/*有故障报警*/
        if(package[2+17]&(1<<0))
            alarmRecordTemp |= Alarm_HMI_communication_station_not_connect;
        else
            alarmRecordTemp &= ~Alarm_HMI_communication_station_not_connect;
        if(package[2+17]&(1<<1))
            alarmRecordTemp |= Alarm_emergency;
        else
            alarmRecordTemp &= ~Alarm_emergency;
        if(package[2+17]&(1<<3))
            alarmRecordTemp |= Alarm_converter_fault;
        else
            alarmRecordTemp &= ~Alarm_converter_fault;
        if(package[2+17]&(1<<5))
            alarmRecordTemp |= Alarm_machine_overshak;
        else
            alarmRecordTemp &= ~Alarm_machine_overshak;
        if(package[2+17]&(1<<7))
            alarmRecordTemp |= Alarm_door_station_abnormal;
        else
            alarmRecordTemp &= ~Alarm_door_station_abnormal;
        if(package[2+18]&(1<<0))
            alarmRecordTemp |= Alarm_door_bolt_input_fault;
        else
            alarmRecordTemp &= ~Alarm_door_bolt_input_fault;
        if(package[2+18]&(1<<1))
            alarmRecordTemp |= Alarm_add_water_overtime;
        else
            alarmRecordTemp &= ~Alarm_add_water_overtime;
        if(package[2+18]&(1<<2))
            alarmRecordTemp |= Alarm_heat_overtime;
        else
            alarmRecordTemp &= ~Alarm_heat_overtime;
        if(package[2+18]&(1<<3))
            alarmRecordTemp |= Alarm_drain_water_overtime;
        else
            alarmRecordTemp &= ~Alarm_drain_water_overtime;
        if(package[2+18]&(1<<4))
            alarmRecordTemp |= Alarm_exceed_temperature;
        else
            alarmRecordTemp &= ~Alarm_exceed_temperature;
        if(package[2+18]&(1<<7))
            alarmRecordTemp |= Alarm_water_level_parameter_not_calbrication;
        else
            alarmRecordTemp &= ~Alarm_water_level_parameter_not_calbrication;
        if(package[2+20]&(1<<0))
            alarmRecordTemp |= Alarm_temperature_sensor_open_circuit;
        else
            alarmRecordTemp &= ~Alarm_temperature_sensor_open_circuit;
        if(package[2+20]&(1<<1))
            alarmRecordTemp |= Alarm_temperature_sensor_short_circuit;
        else
            alarmRecordTemp &= ~Alarm_temperature_sensor_short_circuit;
    }
    qhxyj_devSta.alarmRecord = alarmRecordTemp;
        

}
/**
 * @brief
 * 发送串口数据给主板
 * @param package
 * @param len
 */
int terminal_device_data(uint8_t *buff, uint16_t len)
{
    LYY_LOGD("HAL_vUartSend:");
    lyy_printf_hex(buff,len);
    HAL_vUartSend(HAL_UART_MAIN_BOARD, buff, len,500);
    return 0;
}

/**
 * @brief
 * 乔禾洗衣机下发位状态查询指令
 * @param package
 * @param len
 */ 
int transmit_cmd_bitStatusGet(void)
{
    const uint8_t CMD[] = {0x01,0x01,0x00,0x00,0x00,0xA0,0x3C,0x72};

    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}

/**
 * @brief
 * 乔禾洗衣机下发洗脱运行数据查询指令
 * @param package
 * @param len
 */
int transmit_cmd_washRunDataGet(void)
{
    const uint8_t CMD[] = {0x01 ,0x03 ,0x03 ,0x20 ,0x00 ,0x46 ,0xC5 ,0xB6};

    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}


/**
 * @brief
 * 乔禾洗衣机下发程序选择指令
 * @param package
 * @param len
 */
int transmit_cmd_select_program(uint8_t programNum)
{
    // 01 10 01 32 00 01 02 00 01 73 42
    // 01 10 01 32 00 01 02 00 02 33 43
    // 01 10 01 32 00 01 02 00 03 F2 83
    // 01 10 01 32 00 01 02 00 04 B3 41
    // 01 10 01 32 00 01 02 00 05 72 81
    // 01 10 01 32 00 01 02 00 06 32 80
    uint16_t crc16;
    uint8_t CMD[11] = {0x01,0x10 ,0x01 ,0x32 ,0x00 ,0x01 ,0x02 ,0x00 ,0x01 ,0x73 ,0x42};
    CMD[8] = programNum;
    crc16 = ModBusCRC16_long(CMD, 9);
    CMD[9]  = crc16&0x00FF;
    CMD[10] = (crc16&0xFF00)>>8;
    //LYY_LOGD_hex(CMD,sizeof(CMD));
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}
/**
 * @brief
 * 乔禾洗衣机下发投币个数
 * @param package
 * @param len
 */
static int transmit_cmd_coin_num(uint16_t num)//done
{
    uint16_t crc16;
    uint8_t CMD[11] = {0x01 ,0x10 ,0x01 ,0x31 ,0x00 ,0x01 ,0x02 ,0x00 ,0x00 ,0x00 ,0x00};
    CMD[7] = (num&0xFF00)>>8;
    CMD[8] = (num&0xFF)>>0;
    crc16 = ModBusCRC16_long(CMD, 9);
    CMD[9]  = crc16&0x00FF;
    CMD[10] = (crc16&0xFF00)>>8;
    //LYY_LOGD_hex(CMD,sizeof(CMD));
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}

/**
 * @brief
 * 乔禾洗衣机下发启动指令
 * @param package
 * @param len
 */
int transmit_cmd_start_xyj(void)
{
    const uint8_t CMD[] = {0x01 ,0x10 ,0x01 ,0x2E ,0x00 ,0x01 ,0x02 ,0x00 ,0x01 ,0x71 ,0x1E};
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}

/**
 * @brief
 * 乔禾洗衣机下发下一步指令
 * @param package
 * @param len
 */
int transmit_cmd_nextStep(void)
{
    const uint8_t CMD[] = {0x01 ,0x10 ,0x01 ,0x2F ,0x00 ,0x01 ,0x02 ,0x00 ,0x01 ,0x70 ,0xCF};
    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}

/**
 * @brief
 * 乔禾洗衣机下发停机指令
 * @param package
 * @param len
 */
int transmit_cmd_stopDevice(void)
{
    const uint8_t CMD[] = {0x01 ,0x10 ,0x01 ,0x30 ,0x00 ,0x01 ,0x02 ,0x00 ,0x01 ,0x72 ,0xA0};
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}

/**
 * @brief
 * 乔禾洗衣机下发厂家参数查询指令
 * @param package
 * @param len
 */
int transmit_cmd_factoryParmGet(void)//done
{//01 03 03 E8 00 50 C5 86
    const uint8_t CMD1[] = {0x01 ,0x03 ,0x03 ,0xE8 ,0x00 ,0x64 ,0xC4 ,0x51};
    const uint8_t CMD2[] = {0x01 ,0x03 ,0x04 ,0x4C ,0x00 ,0x64 ,0x84 ,0xC6};
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    terminal_device_data((uint8_t*)CMD1,sizeof(CMD1));
    qhxyjfactoryParamId = 0;
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    return terminal_device_data((uint8_t*)CMD2,sizeof(CMD2));
}

/**
 * @brief 
 * 获取工厂参数--乔禾洗衣机
 * @param time 
 * @return int 
 */
int device_qhxyj_factory_param_get(void)
{

    return transmit_cmd_factoryParmGet();

}

/**
 * @brief
 * 乔禾洗衣机下发厂家参数设置指令
 * @param package
 * @param len
 */
int transmit_cmd_factoryParmSet(uint8_t* data,uint16_t len)//done
{//01 03 03 E8 00 50 C5 86
    uint8_t head[] = {0x01 ,0x10 ,0x03 ,0xE8 ,0x00 ,0x64 ,0xC8};
    uint8_t package[256] ;
    uint16_t crc,i;
    //=======================下发第一包==============================
    memset(package,0,sizeof(package));
    memcpy(package,head,sizeof(head));
    if(data != NULL)
    {
        //大小端转换
        for(i = 0;i < len/4;i++)
        {
            package[sizeof(head)+2*i] = data[2*i+1];
            package[sizeof(head)+2*i+1] = data[2*i];
        }
    }
    crc = ModBusCRC16_long(package,sizeof(head)+len/2);
    package[sizeof(head)+len/2] = crc&0xFF;
    package[sizeof(head) + len/2 +1] = crc>>8;
    terminal_device_data((uint8_t*)package,sizeof(head)+len/2+2);//下发第一包
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    //=======================下发第二包==============================
    memset(package,0,sizeof(package));
    memcpy(package,head,sizeof(head));
    package[2] = 0x04;package[3] = 0x4C;
    if(data != NULL)
    {
        //大小端转换
        for(i = 0;i < len/4;i++)
        {
            package[sizeof(head)+2*i] = data[200+2*i+1];
            package[sizeof(head)+2*i+1] = data[200+2*i];
        }
    }
    crc = ModBusCRC16_long(package,sizeof(head)+len/2);
    package[sizeof(head)+len/2] = crc&0xFF;
    package[sizeof(head) + len/2 +1] = crc>>8;
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    return terminal_device_data((uint8_t*)package,sizeof(head)+len/2+2);
}
/**
 * @brief 
 * 设置工厂参数--乔禾洗衣机
 * @param time 
 * @return int 
 */
int device_qhxyj_factory_param_set(uint8_t* data,uint16_t len)
{

    return transmit_cmd_factoryParmSet( data, len);

}
/**
 * @brief
 * 乔禾洗衣机下发自动运行全局参数查询指令
 * @param package
 * @param len
 */
int transmit_cmd_globalParmGet(void)//done
{//01 03 03 E8 00 50 C5 86
    const uint8_t CMD[] = {0x01 ,0x03 ,0x07 ,0xD6 ,0x00 ,0x1E ,0x25 ,0x4E};
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}

/**
 * @brief 
 * 获取自动程序的全局参数--乔禾洗衣机
 * @param time 
 * @return int 
 */
int device_qhxyj_global_param_get(void)
{

    return transmit_cmd_globalParmGet();

}


int transmit_cmd_programParmSet(uint8_t* data,uint16_t len)//done
{
    uint8_t head[] = {0x01 ,0x10 ,0x07 ,0xCF ,0x00 ,0x01,0x02};
    uint8_t package[256] ;
    uint16_t crc,i;
    memset(package,0,sizeof(package));
    memcpy(package,head,sizeof(head));
    if(data != NULL)
    {
        //大小端转换
        for(i = 0;i < len/2;i++)
        {
            package[sizeof(head)+2*i] = data[2*i+1];
            package[sizeof(head)+2*i+1] = data[2*i];
        }
    }
    crc = ModBusCRC16_long(package,sizeof(head)+len);
    package[sizeof(head)+len] = crc&0xFF;
    package[sizeof(head) + len +1] = crc>>8;
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    return terminal_device_data((uint8_t*)package,sizeof(head)+len+2);
}



/**
 * @brief
 * 乔禾洗衣机下发自动程序的全局参数设置指令
 * @param package
 * @param len
 */
int transmit_cmd_globalParmSet(uint8_t* data,uint16_t len)//done
{//01 03 03 E8 00 50 C5 86
    uint8_t head[] = {0x01 ,0x10 ,0x07 ,0xD6 ,0x00 ,0x1E ,0x3C};
    uint8_t package[256] ;
    uint16_t crc,i;
    memset(package,0,sizeof(package));
    memcpy(package,head,sizeof(head));
    if(data != NULL)
    {
        //大小端转换
        for(i = 0;i < len/2;i++)
        {
            package[sizeof(head)+2*i] = data[2*i+1];
            package[sizeof(head)+2*i+1] = data[2*i];
        }
    }
    crc = ModBusCRC16_long(package,sizeof(head)+len);
    package[sizeof(head)+len] = crc&0xFF;
    package[sizeof(head) + len +1] = crc>>8;
    
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    return terminal_device_data((uint8_t*)package,sizeof(head)+len+2);
}

/**
 * @brief 
 * 设置选择的程序--乔禾洗衣机
 * @param time 
 * @return int 
 */
int device_qhxyj_program_param_set(uint8_t* data,uint16_t len)
{
    uint8_t ret = 0;
    qixyj_setProgram_sem = xSemaphoreCreateBinary();
    transmit_cmd_programParmSet(data, len);
    ret = lyy_semWaitTryV2(qixyj_setProgram_sem, 3000);
    lyy_semFreeV2(qixyj_setProgram_sem);
    qixyj_setProgram_sem = NULL;
    return ret > 0 ? 0 : -1;
}

/**
 * @brief 
 * 设置发自动程序的全局参数--乔禾洗衣机
 * @param time 
 * @return int 
 */
int device_qhxyj_global_param_set(uint8_t* data,uint16_t len)
{

    return transmit_cmd_globalParmSet(data, len);

}
/**
 * @brief
 * 乔禾洗衣机下发第1部分步骤参数查询指令
 * @param package
 * @param len
 */
int transmit_cmd_stepParmGet(uint8_t step)//done
{
    const uint8_t CMD1[] = {0x01 ,0x03 ,0x08 ,0x34 ,0x00 ,0x50 ,0x06 ,0x58};
    const uint8_t CMD2[] = {0x01 ,0x03 ,0x08 ,0x84 ,0x00 ,0x50 ,0x07 ,0xBF};
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;//查询状态定时重置
    if(step == 0) return terminal_device_data((uint8_t*)CMD1,sizeof(CMD2));
   else return terminal_device_data((uint8_t*)CMD2,sizeof(CMD2));
    
}

/**
 * @brief 
 * 获取自动程序的全局参数--乔禾洗衣机
 * @param time 
 * @return int 
 */
int device_qhxyj_step_param_get(uint8_t step)
{
    qhxyjstepParamId = step;
    return transmit_cmd_stepParmGet(step);

}

/**
 * @brief
 * 乔禾洗衣机下发第1部分步骤参数设置指令
 * @param package
 * @param len
 */
int transmit_cmd_stepParmSet(uint8_t step,uint8_t* data,uint16_t len)//done
{
    uint8_t head[] = {0x01 ,0x10 ,0x08 ,0x34 ,0x00 ,0x50 ,0xA0};
    uint8_t package[256] ;
    uint16_t crc,i;
    memset(package,0,sizeof(package));
    memcpy(package,head,sizeof(head));
    if(step == 1) package[3] = 0x84;
    if(data != NULL)
    {
        //大小端转换
        for(i = 0;i < len/2;i++)
        {
            package[sizeof(head)+2*i] = data[2*i+1];
            package[sizeof(head)+2*i+1] = data[2*i];
        }
    }
    crc = ModBusCRC16_long(package,sizeof(head)+len);
    package[sizeof(head)+len] = crc&0xFF;
    package[sizeof(head) + len +1] = crc>>8;
    
    return terminal_device_data((uint8_t*)package,sizeof(head)+len+2);
}
/**
 * @brief 
 * 设置下发第1部分步骤参数--乔禾洗衣机
 * @param time 
 * @return int 
 */
int device_qhxyj_step_param_set(uint8_t step,uint8_t* data,uint16_t len)
{

    return transmit_cmd_stepParmSet(step, data, len);

}
/**
 * @brief
 * 乔禾洗衣机下发故障消除指令
 * @param package
 * @param len
 */
int transmit_cmd_clearFault(void)
{
    const uint8_t CMD[] = {0x01 ,0x10 ,0x01 ,0x2C ,0x00 ,0x01 ,0x02 ,0x00 ,0x01 ,0x70 ,0xFC};
    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}

/**
 * @brief
 * 乔禾洗衣机串口数据帧处理方法
 * @param package
 * @param len
 */
void device_qhxyj_packet_parse(uint8_t *package, uint16_t len)
{
    LYY_LOGD(">>>device_qhxyj_packet_parse[%d]:",len);
    lyy_printf_hex(package,len);
    static uint8_t parmTmp[512];
    uint8_t pragramValue[20*2];
    uint8_t i;
    //LYY_LOGD_hex(package, len);
    if( package[0]==0x01 && (package[1]==0x01) && (package[2] == len-5) )//功能码1-0x01
    {/*位状态应答01 01 14 02 80 00 20 00 00 00 00 00 00 40 00 00 60 00 20 00 00 00 20 AB 85 */
        /*是否有报警、门状态、锁状态、机器是否运行*/
        qhxyj_prase_bitCheckStutus(package,len);

        if(qixyj_checkStaus_sem != NULL){
            lyy_semSignalV2(qixyj_checkStaus_sem);//接收到应答
        }
        flagFristGetSta = 1;
        device_alarm_info_print();
    }
    else if(package[0]==0x01 && (package[1]==0x03) && (package[2] == len-5))//功能码03-0x03
    {
        if(package[2]==0x8C)
        {/*洗脱数据：自动程序剩余时间、当前运行程序编号*/
            qhxyj_washRundata.washTime_min = package[2+(2*(11-1))+1]<<8 | package[2+(2*(11-1))+2];
            qhxyj_washRundata.washTime_sec = package[2+(2*(12-1))+1]<<8 | package[2+(2*(12-1))+2];
            qhxyj_washRundata.washNum      = package[2+(2*(18-1))+1]<<8 | package[2+(2*(18-1))+2];

            qhxyj_account.cur_coin = package[2+(2*(35-1))+1]<<8 | package[2+(2*(35-1))+2];
            qhxyj_account.coin_box = package[2+(2*(36-1))+1]<<8 | package[2+(2*(36-1))+2];
            qhxyj_account.coin_all = package[2+(2*(14-1))+1]<<8 | package[2+(2*(14-1))+2];
            //qhxyj_washRundata.washAllStep  = package[2+(2*(58-1))+1]<<8 | package[2+(2*(58-1))+2];
            // LYY_LOGD("running data>>>>>>>>washTime:%d:%d",qhxyj_washRundata.washTime_min,qhxyj_washRundata.washTime_sec);
            // LYY_LOGD("running data>>>>>>>>washNum:%d",qhxyj_washRundata.washNum);
            // LYY_LOGD("running data>>>>>>>>washAllStep:%d",qhxyj_washRundata.washAllStep);

            if(qixyj_getWashRunData_sem != NULL){
                    lyy_semSignalV2(qixyj_getWashRunData_sem);//接收到应答
            }
        }
        else if(package[2]==0xC8)
        {
            if(qhxyjfactoryParamId == 0)
            {
                memcpy(parmTmp,package+3,package[2]);
                qhxyjfactoryParamId = 1;

            }
            else
            {
                qhxyjfactoryParamId = 0;
                memcpy(parmTmp+200,package+3,package[2]);
                //大小端转换
                for(i = 0;i < 6;i++) //P1 -P6
                {
                    pragramValue[2*i] = parmTmp[95*2+2*i+1];
                    pragramValue[2*i+1] = parmTmp[95*2+2*i];
                } 

                for(i = 0;i < 3;i++) //P7 -P9
                {
                    pragramValue[12+2*i] = parmTmp[150*2+2*i+1];
                    pragramValue[12+2*i+1] = parmTmp[150*2+2*i];
                }

                for(i = 0;i < 2;i++) //P10 -P119
                {
                    pragramValue[18+2*i] = parmTmp[102*2+2*i+1];
                    pragramValue[18+2*i+1] = parmTmp[102*2+2*i];
                }

                pragramValue[22+2*i] = parmTmp[110*2+2*i+1];
                pragramValue[22+2*i+1] = parmTmp[110*2+2*i];

                pragramValue[24+2*i] = parmTmp[116*2+2*i+1];
                pragramValue[24+2*i+1] = parmTmp[116*2+2*i];

                pragramValue[26+2*i] = parmTmp[118*2+2*i+1];
                pragramValue[26+2*i+1] = parmTmp[118*2+2*i];

                memcpy((uint8_t*)&qhxyj_pay_setting.P_Value[0] ,pragramValue,28); 
                lyy_printf_hex(pragramValue,28);

                LYY_LOGD("qhxyj_pay_setting.P1_Value:%d",qhxyj_pay_setting.P_Value[0]); 
                LYY_LOGD("qhxyj_pay_setting.P10_Value:%d",qhxyj_pay_setting.P_Value[9]); 
                LYY_LOGD("qhxyj_pay_setting.P11_Value:%d",qhxyj_pay_setting.P_Value[10]); 
                flagfactoryParmGet = 1;
                qhxyj_ctlb_post_up_raw(parmTmp,package[2]*2);
            }
        }
        else if(package[2]==0x3C) //全局参数
        {
            qhxyj_ctlb_post_up_raw_global(package+3,package[2]);
        }
        else if(package[2]==0xA0) //步骤参数
        {
            qhxyj_ctlb_post_up_raw_step(qhxyjstepParamId,package+3,package[2]);
        }
    }
    else if( package[0]==0x01 && (package[1]==0x10))//功能码16-0x10
    {
        int regAddress = (package[2]<<8) | (package[3]<<0);
        switch(regAddress)
        {
            case 0x012E:
            {/* 启动应答01 10 01 2E 00 01 60 3C*/
                if(qixyj_start_sem != NULL){
                    lyy_semSignalV2(qixyj_start_sem);//接收到应答
                    LYY_LOGD("qixyj_start_sem\r\n");
                }
            }
            break;
            case 0x012F:
            {/* 下一步应答01 10 01 2F 00 01 31 FC */

            }
            break;
            case 0x0130:
            {/*  停止应答01 10 01 30 00 01 00 3A */

            }
            break;
            case 0x0131:
            {/*   投币应答01 10 01 31 00 01 51 FA*/
                if(qhxyj_sendCoin_sem != NULL){
                    lyy_semSignalV2(qhxyj_sendCoin_sem);//接收到应答
                    LYY_LOGD("qhxyj_sendCoin_sem\r\n");
                }
            }
            break;
            case 0x0132:
            {/*程序选择应答01 10 01 32 00 01 A1 FA*/
                if(qixyj_selectProgram_sem != NULL){
                    lyy_semSignalV2(qixyj_selectProgram_sem);//接收到应答
                    LYY_LOGD("qixyj_selectProgram_sem\r\n");
                }
            }
            break;
            case 0x07CF:
            {/*设置选择的程序应答01 10 07 CF 00 01*/
                if(qixyj_setProgram_sem != NULL){
                    lyy_semSignalV2(qixyj_setProgram_sem);//接收到应答
                    LYY_LOGD("qixyj_setProgram_sem\r\n");
                }
            }
            default:break;
        }

    }
    else
    {
        LYY_LOGD("data formate err\r\n");
    }

}



/**
 * @brief 
 * 
 * @param arg 
 */
void thread_qhxyj_inquire_status(void *arg)
{
    (void)arg;
    while (1)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        if(mqtt_status_old != lyy_mqtt_get_status())
        {/*网络状态变更*/
            mqtt_status_old = lyy_mqtt_get_status();
            if(mqtt_status_old == 1)
                flagReportDevStatus = 1;
            else
                flagReportDevStatus = 0;
        }

        if(flagFristGetSta && flagReportDevStatus)
        {/*已经获取过主板状态且网络状态发生上线变更*/
            flagReportDevStatus = 0;
            /* 报一次设备状态*/
            LYY_LOGD("report qhxyj change status>>>>>>>>>>>");
            qhxyj_report_change_status(1);//强制上报
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            transmit_cmd_factoryParmGet();

        }
        else{
            qhxyj_report_change_status(0);//运行/异常状态变更时给后台上报
            if(flagfactoryParmGet == 0)
            {
                
                transmit_cmd_factoryParmGet();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
        
    }
}

/*获取设备状态倒计时清零，立即触发*/
static void trigger_checkSta_timeout(void)
{
    if(flagLoopCheckSta)
        timeout_to_checkSta = 0;
    else
        transmit_cmd_bitStatusGet( );
}

void device_qhxyj_update_status(void *arg)
{
    (void)arg;
    if (flagLoopCheckSta)
    {
        if(timeout_to_checkSta)
        {
            timeout_to_checkSta--;
        }
        if(timeout_to_checkSta<=0)
        {
            transmit_cmd_bitStatusGet( );
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            transmit_cmd_washRunDataGet();
            timeout_to_checkSta = PERIOD_FOR_CHECKSTA/TIME_INTERRUPT_MS;
        }
        
    }
}
HAL_RTOS_TIMER_T g_xyjTimerHandle;

/**
 * @brief 
 * 
 */
void lyy_device_qhxyj_init(void)
{
    //lyy_timerInfor.timer_new_handle(TIME_INTERRUPT_MS,device_qhxyj_update_status,NULL);
    HAL_vRtosTimerCreate(&g_xyjTimerHandle,
                         "MS_TIMER",
                         NULL,
                         TIME_INTERRUPT_MS,
                         1,
                         device_qhxyj_update_status);

    xTaskCreate(    thread_qhxyj_inquire_status, 
                    "thread_qhxyj_inquire_status", 
                    1024*5, //20250610 ZYL jianxiaoneicui gai  4->2
                    NULL, 
                    5,
                    NULL); 
}

/**
 * @brief 
 * 乔禾洗衣机设备账目信息事件上报
 */
void qhxyj_event_post_account(void)//-- done
{
    char order[20] = {"88888888888"};
    cJSON* value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "coin_inc", qhxyj_account.cur_coin);//投币增量
        cJSON_AddNumberToObject(value, "coin_sum", qhxyj_account.coin_all);//投币总数
        cJSON_AddStringToObject(value, "order", order);//订单号
        cJSON_AddStringToObject(value, "pay_method", "offline_pay");//方式
        thing_event_post(value, "accounts");
    }
}

/*--------------------------------------------------------------*/
/**
 * @brief 
 * 乔禾洗衣机设备运行信息上报
 */
void qhxyj_post_dev_status(void)
{
    cJSON* data = cJSON_CreateObject(); 
    cJSON* dev_infor = cJSON_CreateObject();
    if(data != NULL && dev_infor !=NULL)
    {        
        cJSON_AddNumberToObject(data, "run_status", qhxyj_washRundata.washAutoRunSta);
        cJSON_AddNumberToObject(data, "mode",qhxyj_washRundata.washNum);
        cJSON_AddNumberToObject(data, "wash_time_min",qhxyj_washRundata.washTime_min);
        cJSON_AddNumberToObject(data, "wash_time_sec",qhxyj_washRundata.washTime_sec);
        cJSON_AddNumberToObject(data, "errvalue", qhxyj_washRundata.washAlarmInfo);

        cJSON_AddItemToObject(dev_infor, "dev_status", data);
        thing_property_post(dev_infor);        
    }  
}
/**
 * @brief 
 * 乔禾洗衣机设备故障上报
 */
void qhxyj_ctlb_post_up_raw_err(uint8_t errCode)//-- done
{
    uint8_t paramTmp[2];
    
    paramTmp[0] = 0xFF;paramTmp[1] = errCode;

    lyy_thing_ctlb_up_raw(paramTmp,2);
}
/**
 * @brief 
 * 乔禾洗衣机设备工厂参数上报
 */
void qhxyj_ctlb_post_up_raw(uint8_t* data,int datalen)//-- done
{
    uint8_t paramTmp[512];
    uint16_t paramLen;
    int i;
    //组装参数
    memset(paramTmp,0,sizeof(paramTmp));
    //memcpy(paramTmp+1,data,42*2);
    
    paramTmp[0] = 0x05;
    //大小端转换
    for(i = 0;i < datalen/2;i++)
    {
        paramTmp[1+2*i] = data[2*i+1];
        paramTmp[1+2*i+1] = data[2*i];
    }    
    
    paramLen = datalen+1;
    lyy_thing_ctlb_up_raw(paramTmp,paramLen);
}
/**
 * @brief 
 * 乔禾洗衣机设备自动化全局参数上报
 */
void qhxyj_ctlb_post_up_raw_global(uint8_t* data,int datalen)//-- done
{
    uint8_t paramTmp[512];
    uint16_t paramLen;
    int i;
    //组装参数
    memset(paramTmp,0,sizeof(paramTmp));
    //memcpy(paramTmp+1,data,42*2);
    
    paramTmp[0] = 0x30;
//大小端转换
    for(i = 0;i < datalen/2;i++)
    {
        paramTmp[1+2*i] = data[2*i+1];
        paramTmp[1+2*i+1] = data[2*i];
    }    
    
    paramLen = datalen+1;
    lyy_thing_ctlb_up_raw(paramTmp,paramLen);
}
/**
 * @brief 
 * 乔禾洗衣机设备步骤1参数上报
 */
void qhxyj_ctlb_post_up_raw_step(uint8_t step,uint8_t* data,int datalen)//-- done
{
    uint8_t paramTmp[512];
    uint16_t paramLen;
    int i;
    //组装参数
    memset(paramTmp,0,sizeof(paramTmp));
    //memcpy(paramTmp+1,data,42*2);
    
    paramTmp[0] = 0x32;
    if(step == 1) paramTmp[0] = 0x34;
//大小端转换
    for(i = 0;i < datalen/2;i++)
    {
        paramTmp[1+2*i] = data[2*i+1];
        paramTmp[1+2*i+1] = data[2*i];
    }    
    
    paramLen = datalen+1;
    lyy_thing_ctlb_up_raw(paramTmp,paramLen);
}

/**
 * @brief
 * 给后台上报异常状态变更
 * @param package
 * @param len
 */
uint8_t qhxyj_report_change_status(uint8_t force)
{
    uint8_t ret = 0;
    uint8_t reportFlag=0,reportAcountFlag = 0;
    //LYY_LOGD("qhxyj_report_change_status %d\r\n",force);
    //LYY_LOGD("qhxyjCoinCnt_old: %d  qhxyj_account.coin_all %d \r\n",qhxyjCoinCnt_old,qhxyj_account.coin_all);
    if(qhxyjCoinCnt_old!=qhxyj_account.coin_all)
    {
        if(force == 1) 
        {
            qhxyjCoinCnt_old = qhxyj_account.coin_all; //第一次强制上报 
        }
        reportAcountFlag = 1;
    }

    if(alarmRecord_old!=qhxyj_devSta.alarmRecord)
    {/*报警状态变更*/
        //alarmRecord_old = qhxyj_devSta.alarmRecord;
        reportFlag = 1;
    }

    if((qhxyjAtuoRunSta_old != qhxyj_devSta.isAutoRun) || (force==1) )
    {/*运行状态变更*/
        qhxyjAtuoRunSta_old = qhxyj_devSta.isAutoRun;
        reportFlag = 1;
        memset(&qhxyj_washRundata,0,sizeof(TYPE_REPORT_QHXYJ_STA_INFO));//清空待上传数据
        if(qhxyjAtuoRunSta_old==0x01)
        {//正在运行，获取洗脱机运行数据然后上报
            lyy_semNewV2(&qixyj_getWashRunData_sem, 0); 
            transmit_cmd_washRunDataGet();//下发洗脱运行数据查询指令
            ret = lyy_semWaitTryV2(qixyj_getWashRunData_sem, 3000); //等待主板启动应答
            lyy_semFreeV2(qixyj_getWashRunData_sem);
            qixyj_getWashRunData_sem = NULL;
            if(ret>0)
            {//收到主板应答
                LYY_LOGD("washRunDataGet reply ok");
            }
        }
        #if 0
        if(qhxyj_devSta.isAutoRun == 0x01)
        {

            lyy_tjc_page_change_wait();
            LYY_LOGD("lyy_tjc_page_change_wait>>>>>>>>>>>>>>>>");
        }
        else
        {

            lyy_tjc_page_package();
            LYY_LOGD("lyy_tjc_page_package>>>>>>>>>>>>>>>>");
        }
        #endif
    }

    if(reportFlag)
    {
        LYY_LOGD("<<<<<<<<<<<<<<<<<Report State change>>>>>>>>>>>>>>>>");
        qhxyj_washRundata.washAlarmInfo = qhxyj_devSta.alarmRecord;
        qhxyj_washRundata.washAutoRunSta = qhxyj_devSta.isAutoRun;
        LYY_LOGD("running data>>>>>>>>washAutoRunSta:%d",qhxyj_washRundata.washAutoRunSta);
        LYY_LOGD("running data>>>>>>>>washTime:%d:%d",qhxyj_washRundata.washTime_min,qhxyj_washRundata.washTime_sec);
        LYY_LOGD("running data>>>>>>>>washNum:%d",qhxyj_washRundata.washNum);
        LYY_LOGD("running data>>>>>>>>washAlarmInfo:%X",qhxyj_washRundata.washAlarmInfo);
        device_alarm_info_print();
        if(lyy_mqtt_get_status()==1)
        {/*网络正常才上报*/
            qhxyj_post_dev_status( );
            alarmRecord_old = qhxyj_devSta.alarmRecord;
            device_alarm_info_up_raw();
        }
    }
    if(reportAcountFlag)
    {
        LYY_LOGD("<<<<<<<<<<<<<<<<<Report accounts change>>>>>>>>>>>>>>>>");   
        
        if((lyy_mqtt_get_status()==1)&&(qhxyj_devSta.isRun == 1))
        {/*网络正常才上报*/
            if(qhxyj_account.coin_all - qhxyjCoinCnt_old > 0 )
            {
             if(qhxyjCoinCnt_old == 0) //第一次上报
                {
                        if(qhxyj_account.cur_coin < 20)//容错处理
                            qhxyj_event_post_account();
                }
                else
                {
                    qhxyj_account.cur_coin = qhxyj_account.coin_all - qhxyjCoinCnt_old;
                    if(qhxyj_account.cur_coin < 20)//容错处理
                        qhxyj_event_post_account();
                }
            }
            LYY_LOGD("running data>>>>>>>>cur_coin:%d",qhxyj_account.cur_coin);
            LYY_LOGD("running data>>>>>>>>coin_box:%d",qhxyj_account.coin_box);
            LYY_LOGD("running data>>>>>>>>coin_all:%d",qhxyj_account.coin_all); 
            qhxyjCoinCnt_old = qhxyj_account.coin_all;
        } 
         
          
    }
    return ret;
}

/**
 * @brief
 * 支付申请---乔禾洗衣机
 * @param package
 * @param len
 */
int device_qhxyj_pay_apply(void)
{
    uint8_t ret;
    LYY_LOGD("device_qhxyj_pay_apply");   
    lyy_semNewV2(&qixyj_checkStaus_sem, 0); 
    trigger_checkSta_timeout();//transmit_cmd_bitStatusGet( );//给主板发送查询串口指令
    ret = lyy_semWaitTryV2(qixyj_checkStaus_sem, 3000); //等待主板启动应答
    lyy_semFreeV2(qixyj_checkStaus_sem);
    qixyj_checkStaus_sem = NULL;
    LYY_LOGD("device_qhxyj_pay_apply >>>ret = %d >>>lyy_semWaitTryV2 \r\n",ret);   
    if(ret>0)
    {//收到主板应答
        if(qhxyj_devSta.isDoorOk==0 || qhxyj_devSta.isAlarm==1)
        {//上报支付申请异常应答给后台
            LYY_LOGD("qhxyj_pay_check is err");
            apply_start_t.apply_result = 0x00;//0x00-失败 0x01-成功
        }
        else
        {//上报支付申请正常应答给后台
            LYY_LOGD("qhxyj_pay_check is ok");
            apply_start_t.apply_result = 0x01;//0x00-失败 0x01-成功
        }
    }

    return ret;
}

/**
 * @brief 
 * 获取程序对应币值
 * @param mode 
 * @return int 
 */
int get_programNum_value(uint16_t mode)
{
    uint16_t value = 99;

    value = qhxyj_pay_setting.P_Value[mode-1];
    return value;
}

/**
 * @brief 
 * 支付启动--乔禾洗衣机
 * @param time 
 * @return int 
 */
int device_qhxyj_pay_start(uint16_t mode)
{
    int repeatCnt = 0;
    uint16_t coinNun = 0;
    uint8_t ret = 0;
    /*给主板下发程序选择指令*/
    //lyy_semNewV2(&qixyj_selectProgram_sem, 0);
    qixyj_selectProgram_sem = xSemaphoreCreateBinary();
    transmit_cmd_select_program(mode);
    /*等待程序选择应答指令*/
    ret = lyy_semWaitTryV2(qixyj_selectProgram_sem, 3000);
    lyy_semFreeV2(qixyj_selectProgram_sem);
    qixyj_selectProgram_sem = NULL;
    if(ret==0)
    {//超时未收到主板应答
        event_post_log("transmit_cmd_select_program");
        return 0;
    }
    vTaskDelay(200 / portTICK_PERIOD_MS); // 延时一会
    /*给主板下发投币指令*/
    lyy_semNewV2(&qhxyj_sendCoin_sem, 0);
    coinNun = get_programNum_value(mode);
    transmit_cmd_coin_num(coinNun);
    /*等待启动应答指令*/
    ret = lyy_semWaitTryV2(qhxyj_sendCoin_sem, 3000);
    lyy_semFreeV2(qhxyj_sendCoin_sem);
    qhxyj_sendCoin_sem = NULL;
    if(ret==0)
    {//超时未收到主板应答
        event_post_log("transmit_cmd_coin_num");
        return 0;
    }

    // transmit_cmd_washRunDataGet();
    vTaskDelay(500 / portTICK_PERIOD_MS); // 延时一会
    // qhxyjCoinCnt_old+=coinNun;
    // qhxyj_report_change_status(0);


    #if 0
    /*给主板下发启动指令*/
    lyy_semNewV2(&qixyj_start_sem, 0);
    transmit_cmd_start_xyj( );
    /*等待启动应答指令*/
    ret = lyy_semWaitTryV2(qixyj_start_sem, 3000);
    lyy_semFreeV2(qixyj_start_sem);
    qixyj_start_sem = NULL;
    #endif
    while(1)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS); // 延时一会
        lyy_semNewV2(&qixyj_checkStaus_sem, 0); 
        trigger_checkSta_timeout();//transmit_cmd_bitStatusGet( );//给主板发送查询串口指令
        ret = lyy_semWaitTryV2(qixyj_checkStaus_sem, 3000); //等待主板启动应答
        lyy_semFreeV2(qixyj_checkStaus_sem);
        qixyj_checkStaus_sem = NULL;
        if(ret>0)
        {//收到主板应答
            if(qhxyj_devSta.isAutoRun==1){
                apply_start_t.start_result = 0x01;//0x00-失败 0x01-成功
                break;
            }
            else{ 
                apply_start_t.start_result = 0x00;//0x00-失败 0x01-成功
                event_post_log("apply_start_t.start_result = 0x00");
                lyy_semNewV2(&qixyj_getWashRunData_sem, 0); 
                transmit_cmd_washRunDataGet();//下发洗脱运行数据查询指令
                ret = lyy_semWaitTryV2(qixyj_getWashRunData_sem, 3000); //等待主板启动应答
                lyy_semFreeV2(qixyj_getWashRunData_sem);
                qixyj_getWashRunData_sem = NULL;
                if(qhxyj_account.cur_coin != coinNun)
                {
                    /*给主板下发投币指令*/
                    lyy_semNewV2(&qhxyj_sendCoin_sem, 0);
                    coinNun = get_programNum_value(mode);
                    transmit_cmd_coin_num(coinNun);
                    /*等待启动应答指令*/
                    ret = lyy_semWaitTryV2(qhxyj_sendCoin_sem, 3000);
                    lyy_semFreeV2(qhxyj_sendCoin_sem);
                    qhxyj_sendCoin_sem = NULL;
                }
                vTaskDelay(3000 / portTICK_PERIOD_MS); // 3秒后重新获取
                repeatCnt++;
            }
        }
        else
        {
            repeatCnt++;
        }
        if(repeatCnt>3)
        {
            event_post_log("repeatCnt=3");
            break;
        }
    }
    
    return ret;
}






