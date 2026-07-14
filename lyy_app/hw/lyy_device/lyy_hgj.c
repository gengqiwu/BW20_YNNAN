/*
 * @author: e.he
 * @Date: 2024-07-10 16:29:48
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:31:10
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
#include "lyy_hgj.h"
#include "lyy_device.h"
#include "lyy_tjc.h"

#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

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
// #include "sys_packet.h"
// #include "os_inter.h"
#include "lyy_para.h"
// #include "bsp_osInter.h"

static uint8_t flagLoopCheckSta = 1;    // 循环获取状态启动标志
static uint8_t flagFristGetSta = 0;     // 首次获取到主板状态标志
static uint8_t flagReportDevStatus = 0; // 满足上报主板状态标志
static int flagfactoryParmGet = 0;          // 记录主板参数获取标记
static int mqtt_status_old = 0;             // 记录mqtt连接状态

static SEM_TYPE qhhgj_checkStaus_sem;
static SEM_TYPE qhhgj_selectProgram_sem;
static SEM_TYPE qhhgj_sendCoin_sem;
static SEM_TYPE qhhgj_start_sem;
static SEM_TYPE qhhgj_getrunData_sem;

static uint32_t alarmRecord_old = 0;

TYPE_QHHGJ_DEV_FACTORY_PARM qhhgj_devParm = {0}; // 厂家参数
TYPE_QHHGJ_DEV_STA qhhgj_devSta = {0};           // 状态信息
TYPE_REPORT_QHHGJ_STA_INFO qhghj_rundata = {0};
TYPE_REPORT_QHHGJ_ACCOUNT_INFO qhhgj_account = {0}; // 投币账目统计
uint8_t qhhgjRunSta_old = 0;                    // 对应qhhgj_devSta.isRun
uint16_t qhhgjCoinCnt_old = 0;                  // 对应qhhgj_account.coin_all;
uint8_t qhhgjfactoryParamId = 0;                    // 控制读取参数序号
#define TIME_INTERRUPT_MS (50U)              // 定时器周期
#define PERIOD_FOR_CHECKSTA (3000U)          // 获取设备状态的周期
static int timeout_to_checkSta = PERIOD_FOR_CHECKSTA / TIME_INTERRUPT_MS;
/*******************************************************************************
 *函数名称：uint16_t ModBusCRC16_long(uint8_t *cmd, unsigned long len)
 *函数说明：
 *输入参数：
 *输出参数：
 *返 回 值：
 *其他说明：CRC16 ModBus 协议
 *******************************************************************************/
static uint16_t ModBusCRC16_long(uint8_t *cmd, unsigned long len) // done
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
                CRC16 ^= 0xA001; // 异或多项式
            }
        }
        // 4. 重复3直至8位全部移位计算结束；
    }
    // 5. 重复将所有输入数据操作完成以上步骤，所得16位数即16位CRC校验码。
    return CRC16;
}

/*报警信息打印 */
static void device_alarm_info_print(void) // done
{
    if (qhhgj_devSta.isAlarm)
    {
        LYY_LOGD("!!!!!!!!!Alarm!!!!!!!!! qhhgj_devSta.alarmRecord:%X", qhhgj_devSta.alarmRecord);
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_communication_station_not_connect)
            LYY_LOGD("Alarm_HGJ_communication_station_not_connect"); //: 通信失败
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_emergency)
            LYY_LOGD("Alarm_HGJ_emergency"); // 急停报警
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_moter_overload)
            LYY_LOGD("Alarm_HGJ_moter_overload"); // 过载
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_electrical_overheat)
            LYY_LOGD("Alarm_HGJ_electrical_overheat"); // 电过热
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_air_outlet_temperature_overheat)
            LYY_LOGD("Alarm_HGJ_air_outlet_temperature_overheat"); // 出风口温度过高
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_temperature_sensor_open_circuit)
            LYY_LOGD("Alarm_HGJ_temperature_sensor_open_circuit"); // 温度探头断开
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_temperature_sensor_short_circuit)
            LYY_LOGD("Alarm_HGJ_temperature_sensor_short_circuit"); // 温度探头短路
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_inlet_temperature_sensor_broken)
            LYY_LOGD("Alarm_HGJ_inlet_temperature_sensor_broken"); // 进风口温度探头损坏
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_air_inlet_temperature_overheat)
            LYY_LOGD("Alarm_HGJ_air_inlet_temperature_overheat"); // 进风口温度过高
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_flame_fault)
            LYY_LOGD("Alarm_HGJ_flame_fault"); // 点火失败
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_fandoor_open)
            LYY_LOGD("Alarm_HGJ_fandoor_open"); // 风门开
    }
}

/*报警信息上送 */
static void device_alarm_info_up_raw(void) // done
{
    if (qhhgj_devSta.isAlarm)
    {
        LYY_LOGD("!!!!!!!!!Alarm!!!!!!!!! qhhgj_devSta.alarmRecord:%X", qhhgj_devSta.alarmRecord);
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_communication_station_not_connect)
        {
            LYY_LOGD("Alarm_HGJ_communication_station_not_connect"); //: 通信失败
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_communication_station_not_connect);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_emergency)
        {
            LYY_LOGD("Alarm_HGJ_emergency"); // 急停报警
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_emergency);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_moter_overload)
        {
            LYY_LOGD("Alarm_HGJ_moter_overload"); // 过载
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_moter_overload);
        }

        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_electrical_overheat)
        {
            LYY_LOGD("Alarm_HGJ_electrical_overheat"); // 电过热
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_electrical_overheat);
        }

        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_air_outlet_temperature_overheat)
        {
            LYY_LOGD("Alarm_HGJ_air_outlet_temperature_overheat"); // 出风口温度过高
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_air_outlet_temperature_overheat);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_temperature_sensor_open_circuit)
        {
            LYY_LOGD("Alarm_HGJ_temperature_sensor_open_circuit"); // 温度探头断开
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_temperature_sensor_open_circuit);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_temperature_sensor_short_circuit)
        {
            LYY_LOGD("Alarm_HGJ_temperature_sensor_short_circuit"); // 温度探头短路
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_temperature_sensor_short_circuit);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_inlet_temperature_sensor_broken)
        {
            LYY_LOGD("Alarm_HGJ_inlet_temperature_sensor_broken"); // 进风口温度探头损坏
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_inlet_temperature_sensor_broken);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_air_inlet_temperature_overheat)
        {
            LYY_LOGD("Alarm_HGJ_air_inlet_temperature_overheat"); // 进风口温度过高
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_air_inlet_temperature_overheat);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_flame_fault)
        {
            LYY_LOGD("Alarm_HGJ_flame_fault"); // 点火失败
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_flame_fault);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_fandoor_open)
        {
            LYY_LOGD("Alarm_HGJ_fandoor_open"); // 风门开
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_fandoor_open);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_temperature_fault)
        {
            LYY_LOGD("Alarm_HGJ_temperature_fault"); // 温度探头故障
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_temperature_fault);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_sprinkler_sensor_fault)
        {
            LYY_LOGD("Alarm_HGJ_sprinkler_sensor_fault"); // 喷淋探头故障
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_sprinkler_sensor_fault);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_cylinder_temperature_too_high)
        {
            LYY_LOGD("Alarm_HGJ_cylinder_temperature_too_high"); // 内筒温度过高
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_cylinder_temperature_too_high);
        }
        if (qhhgj_devSta.alarmRecord & Alarm_HGJ_check_no_cylinder_run)
        {
            LYY_LOGD("Alarm_HGJ_check_no_cylinder_run"); // 内筒温度过高
            qhhgj_ctlb_post_up_raw_err(ErrCode_HGJ_check_no_cylinder_run);
        }
    }
}

/**
 * @brief
 * 乔禾烘干机位状态解析
 * @param pay_id
 * @param sub_id
 * @param id
 * @return
 */
void qhhgj_prase_bitCheckStutus(uint8_t *package, uint16_t len) // done
{
    (void)len;

    LYY_LOGD("bit status>>>>>>>>>>>>>>>>BYTE1:0x%X", package[2 + 1]);
    LYY_LOGD("bit status>>>>>>>>>>>>>>>>BYTE2:0x%X", package[2 + 2]);
    LYY_LOGD("bit status>>>>>>>>>>>>>>>>BYTE3:0x%X", package[2 + 3]);
    LYY_LOGD("bit status>>>>>>>>>>>>>>>>BYTE4:0x%X", package[2 + 4]);
    LYY_LOGD("bit status>>>>>>>>>>>>>>>>BYTE5:0x%X", package[2 + 5]);
    LYY_LOGD("bit status>>>>>>>>>>>>>>>>BYTE6:0x%X", package[2 + 6]);
    qhhgj_devSta.isRun = (package[2 + 3] & (1 << 0)) ? 1 : 0;
    qhhgj_devSta.isAlarm = (package[2 + 3] & (1 << 1)) ? 1 : 0;
    qhhgj_devSta.isReady = (package[2 + 3] & (1 << 2)) ? 1 : 0;
    LYY_LOGD("    qhxyj_devSta.isRun :%d", qhhgj_devSta.isRun);
    LYY_LOGD("  qhxyj_devSta.isAlarm :%d", qhhgj_devSta.isAlarm);
    LYY_LOGD("  qhxyj_devSta.isReady :%d", qhhgj_devSta.isReady);

    uint32_t alarmRecordTemp = qhhgj_devSta.alarmRecord;
    if (qhhgj_devSta.isAlarm == 0)
    { /*无故障报警*/
        alarmRecordTemp = 0;
    }
    else
    { /*有故障报警*/
        if (package[2 + 5] & (1 << 0))
            alarmRecordTemp |= Alarm_HGJ_communication_station_not_connect;
        else
            alarmRecordTemp &= ~Alarm_HGJ_communication_station_not_connect;
        if (package[2 + 5] & (1 << 1))
            alarmRecordTemp |= Alarm_HGJ_emergency;
        else
            alarmRecordTemp &= ~Alarm_HGJ_emergency;
        if (package[2 + 5] & (1 << 2))
            alarmRecordTemp |= Alarm_HGJ_moter_overload;
        else
            alarmRecordTemp &= ~Alarm_HGJ_moter_overload;
        if (package[2 + 5] & (1 << 3))
            alarmRecordTemp |= Alarm_HGJ_electrical_overheat;
        else
            alarmRecordTemp &= ~Alarm_HGJ_electrical_overheat;
        if (package[2 + 5] & (1 << 4))
            alarmRecordTemp |= Alarm_HGJ_air_outlet_temperature_overheat;
        else
            alarmRecordTemp &= ~Alarm_HGJ_air_outlet_temperature_overheat;
        if (package[2 + 5] & (1 << 5))
            alarmRecordTemp |= Alarm_HGJ_temperature_sensor_open_circuit;
        else
            alarmRecordTemp &= ~Alarm_HGJ_temperature_sensor_open_circuit;
        if (package[2 + 5] & (1 << 6))
            alarmRecordTemp |= Alarm_HGJ_temperature_sensor_short_circuit;
        else
            alarmRecordTemp &= ~Alarm_HGJ_temperature_sensor_short_circuit;
        if (package[2 + 5] & (1 << 7))
            alarmRecordTemp |= Alarm_HGJ_inlet_temperature_sensor_broken;
        else
            alarmRecordTemp &= ~Alarm_HGJ_inlet_temperature_sensor_broken;
        if (package[2 + 6] & (1 << 0))
            alarmRecordTemp |= Alarm_HGJ_air_inlet_temperature_overheat;
        else
            alarmRecordTemp &= ~Alarm_HGJ_air_inlet_temperature_overheat;
        if (package[2 + 6] & (1 << 1))
            alarmRecordTemp |= Alarm_HGJ_flame_fault;
        else
            alarmRecordTemp &= ~Alarm_HGJ_flame_fault;
        if (package[2 + 6] & (1 << 2))
            alarmRecordTemp |= Alarm_HGJ_fandoor_open;
        else
            alarmRecordTemp &= ~Alarm_HGJ_fandoor_open;
        if (package[2 + 6] & (1 << 3))
            alarmRecordTemp |= Alarm_HGJ_temperature_fault;
        else
            alarmRecordTemp &= ~Alarm_HGJ_temperature_fault;
        if (package[2 + 6] & (1 << 4))
            alarmRecordTemp |= Alarm_HGJ_sprinkler_sensor_fault;
        else
            alarmRecordTemp &= ~Alarm_HGJ_sprinkler_sensor_fault;
        if (package[2 + 6] & (1 << 5))
            alarmRecordTemp |= Alarm_HGJ_cylinder_temperature_too_high;
        else
            alarmRecordTemp &= ~Alarm_HGJ_cylinder_temperature_too_high;
        if (package[2 + 6] & (1 << 6))
            alarmRecordTemp |= Alarm_HGJ_check_no_cylinder_run;
        else
            alarmRecordTemp &= ~Alarm_HGJ_check_no_cylinder_run;
    }
    qhhgj_devSta.alarmRecord = alarmRecordTemp;
}

/**
 * @brief
 * 发送串口数据给主板
 * @param package
 * @param len
 */
static int terminal_device_data(uint8_t *buff, uint16_t len) // done
{

    LYY_LOGD("HAL_vUartSend:");
    lyy_printf_hex(buff, len);
    HAL_vUartSend(HAL_UART_MAIN_BOARD, buff, len, 500);
    return 0;
}

/**
 * @brief
 * 乔禾烘干机下发位状态查询指令
 * @param package
 * @param len
 */
static int transmit_cmd_bitStatusGet(void) // done
{
    const uint8_t CMD[] = {0x01, 0x01, 0x00, 0x00, 0x00, 0xA0, 0x3C, 0x72};

    return terminal_device_data((uint8_t *)CMD, sizeof(CMD));
}

/**
 * @brief
 * 乔禾烘干机下发运行数据查询指令
 * @param package
 * @param len
 */
static int transmit_cmd_runDataGet(void) // done
{
    int ret = 0;
    const uint8_t CMD[] = {0x01, 0x03, 0x03, 0x20, 0x00, 0x46, 0xC5, 0xB6};
    ret = terminal_device_data((uint8_t *)CMD, sizeof(CMD));
    return ret;
}

/**
 * @brief
 * 乔禾烘干机下发厂家参数查询指令
 * @param package
 * @param len
 */
static int transmit_cmd_factoryParmGet(void) // done
{                                            // 01 03 03 E8 00 50 C5 86
    // const uint8_t CMD[] = {0x01 ,0x03 ,0x03 ,0xE8 ,0x00 ,0x50 ,0xC5 ,0x86};
    // return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
    const uint8_t CMD1[] = {0x01, 0x03, 0x03, 0xE8, 0x00, 0x50, 0xC5, 0x86};
    const uint8_t CMD2[] = {0x01, 0x03, 0x04, 0x38, 0x00, 0x50, 0xc5, 0x0B};
    int result = 0;
    timeout_to_checkSta = PERIOD_FOR_CHECKSTA / TIME_INTERRUPT_MS; // 查询状态定时重置
    result = terminal_device_data((uint8_t *)CMD1, sizeof(CMD1));
    qhhgjfactoryParamId = 0;
    if (get_dev_type_code() == DEVICE_TYPE_UART_QHHGJ)
        return result;

    vTaskDelay(1000 / portTICK_PERIOD_MS);

    timeout_to_checkSta = PERIOD_FOR_CHECKSTA / TIME_INTERRUPT_MS; // 查询状态定时重置
    return terminal_device_data((uint8_t *)CMD2, sizeof(CMD2));
}

/**
 * @brief
 * 获取工厂参数--乔禾烘干机
 * @param time
 * @return int
 */
int device_qhhgj_factory_param_get(void)
{

    return transmit_cmd_factoryParmGet();
}

/**
 * @brief
 * 乔禾烘干机下发厂家参数设置指令
 * @param package
 * @param len
 */
static int transmit_cmd_factoryParmSet(uint8_t *data, uint16_t len) // done
{                                                                           // 01 03 03 E8 00 50 C5 86
    const uint8_t head1[] = {0x01, 0x10, 0x03, 0xE8, 0x00, 0x50, 0xA0};
    const uint8_t head2[] = {0x01, 0x10, 0x04, 0x38, 0x00, 0x50, 0xA0};
    uint8_t package1[256];
    uint8_t package2[256];
    uint16_t crc1, crc2, i;
    memset(package1, 0, sizeof(package1));
    memset(package2, 0, sizeof(package2));
    memcpy(package1, head1, sizeof(head1));
    memcpy(package2, head2, sizeof(head2));
    if (data != NULL)
    {
        // 大小端转换
        for (i = 0; i < len / 4; i++)
        {
            package1[sizeof(head1) + 2 * i] = data[2 * i + 1];
            package1[sizeof(head1) + 2 * i + 1] = data[2 * i];
            package2[sizeof(head2) + 2 * i] = data[len / 2 + 2 * i + 1];
            package2[sizeof(head2) + 2 * i + 1] = data[len / 2 + 2 * i];
        }
        crc1 = ModBusCRC16_long(package1, sizeof(head1) + len / 2);
        crc2 = ModBusCRC16_long(package2, sizeof(head2) + len / 2);
        package1[sizeof(head1) + len / 2] = crc1 & 0xFF;
        package2[sizeof(head2) + len / 2] = crc2 & 0xFF;
        package1[sizeof(head1) + len / 2 + 1] = crc1 >> 8;
        package2[sizeof(head2) + len / 2 + 1] = crc2 >> 8;
        terminal_device_data((uint8_t *)package1, sizeof(head1) + len / 2 + 2);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        terminal_device_data((uint8_t *)package2, sizeof(head2) + len / 2 + 2);
    }
    return 0;
}

/**
 * @brief
 * 乔禾烘干机下发厂家参数设置指令
 * @param package
 * @param len
 */
static int transmit_cmd_factoryParmSet_forqhhgj(uint8_t *data, uint16_t len) // done
{                                                                   // 01 03 03 E8 00 50 C5 86
    const uint8_t head[] = {0x01, 0x10, 0x03, 0xE8, 0x00, 0x50, 0xA0};
    uint8_t package[256];
    uint16_t crc, i;
    memset(package, 0, sizeof(package));
    memcpy(package, head, sizeof(head));
    if (data != NULL)
    {
        // 大小端转换
        for (i = 0; i < len / 2; i++)
        {
            package[sizeof(head) + 2 * i] = data[2 * i + 1];
            package[sizeof(head) + 2 * i + 1] = data[2 * i];
        }
    }
    crc = ModBusCRC16_long(package, sizeof(head) + len);
    package[sizeof(head) + len] = crc & 0xFF;
    package[sizeof(head) + len + 1] = crc >> 8;
    return terminal_device_data((uint8_t *)package, sizeof(head) + len + 2);
}
/**
 * @brief
 * 设置工厂参数--乔禾烘干机
 * @param time
 * @return int
 */
int device_qhhgj_factory_param_set(uint8_t *data, uint16_t len)
{

    return transmit_cmd_factoryParmSet(data, len);
}

int device_qhhgj_factory_param_set_forqhhgj(uint8_t *data, uint16_t len)
{

    return transmit_cmd_factoryParmSet_forqhhgj(data, len);
}
/**
 * @brief
 * 乔禾烘干机下发程序选择指令,范围1~3
 * @param package
 * @param len
 */
static int transmit_cmd_select_program(uint8_t programNum) // done
{
    // 01 10 01 32 00 01 02 00 01 73 42
    // 01 10 01 32 00 01 02 00 02 33 43
    // 01 10 01 32 00 01 02 00 03 F2 83
    uint16_t crc16;
    uint8_t CMD[11] = {0x01, 0x10, 0x01, 0x32, 0x00, 0x01, 0x02, 0x00, 0x01, 0x73, 0x42};
    CMD[8] = programNum;
    crc16 = ModBusCRC16_long(CMD, 9);
    CMD[9] = crc16 & 0x00FF;
    CMD[10] = (crc16 & 0xFF00) >> 8;
    // LYY_LOGD_hex(CMD,sizeof(CMD));
    return terminal_device_data((uint8_t *)CMD, sizeof(CMD));
}

/**
 * @brief
 * 乔禾烘干机下发投币个数
 * @param package
 * @param len
 */
static int transmit_cmd_coin_num(uint16_t num) // done
{
    uint16_t crc16;
    uint8_t CMD[11] = {0x01, 0x10, 0x01, 0x31, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x00};
    CMD[7] = (num & 0xFF00) >> 8;
    CMD[8] = (num & 0xFF) >> 0;
    crc16 = ModBusCRC16_long(CMD, 9);
    CMD[9] = crc16 & 0x00FF;
    CMD[10] = (crc16 & 0xFF00) >> 8;
    // LYY_LOGD_hex(CMD,sizeof(CMD));
    return terminal_device_data((uint8_t *)CMD, sizeof(CMD));
}

/**
 * @brief
 * 乔禾烘干机下发启动指令
 * @param package
 * @param len
 */
static int transmit_cmd_start_hgj(void) // done
{
    const uint8_t CMD[] = {0x01, 0x10, 0x01, 0x2E, 0x00, 0x01, 0x02, 0x00, 0x01, 0x71, 0x1E};
    return terminal_device_data((uint8_t *)CMD, sizeof(CMD));
}

#if 0
/**
 * @brief
 * 乔禾烘干机下发下一步指令
 * @param package
 * @param len
 */
static int transmit_cmd_nextStep(void)//done
{
    const uint8_t CMD[] = {0x01 ,0x10 ,0x01 ,0x2F ,0x00 ,0x01 ,0x02 ,0x00 ,0x01 ,0x70 ,0xCF};
    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}


/**
 * @brief
 * 乔禾烘干机下发故障消除指令
 * @param package
 * @param len
 */
static int transmit_cmd_clearFault(void)//done
{
    const uint8_t CMD[] = {0x01 ,0x10 ,0x01 ,0x2C ,0x00 ,0x01 ,0x02 ,0x00 ,0x01 ,0x70 ,0xFC};
    return terminal_device_data((uint8_t*)CMD,sizeof(CMD));
}
#endif
/**
 * @brief
 * 烘干机串口数据帧处理方法
 * @param package
 * @param len
 */
void device_qhhgj_packet_parse(uint8_t *package, uint16_t len)
{
    static uint8_t parmTmp[512];
    LYY_LOGD(">>>device_qhhgj_packet_parse:");
    lyy_printf_hex(package, len);
    // LYY_LOGD_hex(package, len);
    if (package[0] == 0x01 && (package[1] == 0x01) && (package[2] == len - 5)) // 功能码1-0x01
    {                                                                          /*位状态应答01 01 14 00 00 4C 01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 87 A9 */
        /*是否有报警、门状态、锁状态、机器是否运行*/
        qhhgj_prase_bitCheckStutus(package, len);

        if (qhhgj_checkStaus_sem != NULL)
        {
            lyy_semSignalV2(qhhgj_checkStaus_sem); // 接收到应答
        }
        flagFristGetSta = 1;
        device_alarm_info_print();
    }
    else if (package[0] == 0x01 && (package[1] == 0x03) && (package[2] == len - 5)) // 功能码03-0x03
    {
        if (package[2] == 0x8C)
        { /*烘干机运行数据：自动程序剩余时间、当前运行程序编号*/
            qhghj_rundata.time_min = package[2 + (2 * (6 - 1)) + 1] << 8 | package[2 + (2 * (6 - 1)) + 2];
            qhghj_rundata.time_sec = package[2 + (2 * (7 - 1)) + 1] << 8 | package[2 + (2 * (7 - 1)) + 2];

            qhhgj_account.cur_coin = package[2 + (2 * (34 - 1)) + 1] << 8 | package[2 + (2 * (34 - 1)) + 2];
            qhhgj_account.coin_box = package[2 + (2 * (35 - 1)) + 1] << 8 | package[2 + (2 * (35 - 1)) + 2];
            qhhgj_account.coin_all = package[2 + (2 * (36 - 1)) + 1] << 8 | package[2 + (2 * (36 - 1)) + 2];
            // qhghj_rundata.washNum      = package[2+(2*(18-1))+1]<<8 | package[2+(2*(18-1))+2];
            // qhxyj_washRundata.washAllStep  = package[2+(2*(58-1))+1]<<8 | package[2+(2*(58-1))+2];
            LYY_LOGD("running data>>>>>>>>cur_coin:%d", qhhgj_account.cur_coin);
            LYY_LOGD("running data>>>>>>>>coin_box:%d", qhhgj_account.coin_box);
            LYY_LOGD("running data>>>>>>>>coin_all:%d", qhhgj_account.coin_all);

            if (qhhgj_getrunData_sem != NULL)
            {
                lyy_semSignalV2(qhhgj_getrunData_sem); // 接收到应答
            }
        }
        else if (package[2] == 0xA0)
        { /*烘干机厂家参数*/

            // if(get_dev_type_code() == DEVICE_TYPE_UART_QHHGJ)
            if (qhhgjfactoryParamId == 0)
            {

                qhhgj_devParm.coinPerPulse = package[2 + (2 * (61 - 1)) + 1] << 8 | package[2 + (2 * (61 - 1)) + 2];
                qhhgj_devParm.baseCoinCnt = package[2 + (2 * (63 - 1)) + 1] << 8 | package[2 + (2 * (63 - 1)) + 2];
                qhhgj_devParm.minAddCoinCnt = package[2 + (2 * (65 - 1)) + 1] << 8 | package[2 + (2 * (65 - 1)) + 2];
                qhhgj_devParm.hightAddCoinCnt = package[2 + (2 * (66 - 1)) + 1] << 8 | package[2 + (2 * (66 - 1)) + 2];

                if (get_dev_type_code() == DEVICE_TYPE_UART_LJHGJ)
                {
                    qhhgj_devParm.coinPerPulse = qhhgj_devParm.coinPerPulse / 100;
                    memcpy(parmTmp, package + 3, package[2]);
                    qhhgjfactoryParamId = 1;
                }
                else
                {
                    flagfactoryParmGet = 1;
                    qhhgjfactoryParamId = 0;
                    qhhgj_ctlb_post_up_raw(package + 3, package[2]);
                }
                LYY_LOGD("factory data>>>>>>>>coinPerPulse:%d", qhhgj_devParm.coinPerPulse);
                LYY_LOGD("factory data>>>>>>>>baseCoinCnt:%d", qhhgj_devParm.baseCoinCnt);
                LYY_LOGD("factory data>>>>>>>>minAddCoinCnt:%d", qhhgj_devParm.minAddCoinCnt);
                LYY_LOGD("factory data>>>>>>>>hightAddCoinCnt:%d", qhhgj_devParm.hightAddCoinCnt);
            }
            else
            {
                qhhgjfactoryParamId = 0;
                lyy_printf_hex(package + 2 + 50 * 2, 20);
                qhhgj_devParm.lowCoinCnt = package[2 + (2 * (125 - 80 - 1)) + 1] << 8 | package[2 + (2 * (125 - 80 - 1)) + 2];
                qhhgj_devParm.minCoinCnt = package[2 + (2 * (133 - 80 - 1)) + 1] << 8 | package[2 + (2 * (133 - 80 - 1)) + 2];
                qhhgj_devParm.hightCoinCnt = package[2 + (2 * (131 - 80 - 1)) + 1] << 8 | package[2 + (2 * (131 - 80 - 1)) + 2];
                LYY_LOGD("factory data>>>>>>>>lowCoinCnt:%d", qhhgj_devParm.lowCoinCnt);
                LYY_LOGD("factory data>>>>>>>>minCoinCnt:%d", qhhgj_devParm.minCoinCnt);
                LYY_LOGD("factory data>>>>>>>>hightCoinCnt:%d", qhhgj_devParm.hightCoinCnt);
                flagfactoryParmGet = 1;
                memcpy(parmTmp + 160, package + 3, package[2]);
                qhhgj_ctlb_post_up_raw(parmTmp, package[2] + 160);
            }
        }
    }
    else if (package[0] == 0x01 && (package[1] == 0x10)) // 功能码16-0x10
    {
        int regAddress = (package[2] << 8) | (package[3] << 0);
        switch (regAddress)
        {
        case 0x012E:
        { /* 启动应答01 10 01 2E 00 01 60 3C*/
            if (qhhgj_start_sem != NULL)
            {
                lyy_semSignalV2(qhhgj_start_sem); // 接收到应答
            }
        }
        break;
        case 0x012F:
        { /* 下一步应答01 10 01 2F 00 01 31 FC */
        }
        break;
        case 0x0131:
        { /*   投币应答01 10 01 31 00 01 51 FA*/
            if (qhhgj_sendCoin_sem != NULL)
            {
                lyy_semSignalV2(qhhgj_sendCoin_sem); // 接收到应答
            }
        }
        break;
        case 0x0132:
        { /*程序选择应答01 10 01 32 00 01 A1 FA*/
            if (qhhgj_selectProgram_sem != NULL)
            {
                lyy_semSignalV2(qhhgj_selectProgram_sem); // 接收到应答
            }
        }
        break;
        default:
            break;
        }
    }
    else
    {
        LYY_LOGD("数据格式有误");
    }
}

/**
 * @brief
 *
 * @param arg
 */
void thread_qhhgj_inquire_status(void *arg) //-- done
{
    (void)arg;
    while (1)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        if (mqtt_status_old != lyy_mqtt_get_status())
        { /*网络状态变更*/
            mqtt_status_old = lyy_mqtt_get_status();
            if (mqtt_status_old == 1)
                flagReportDevStatus = 1;
            else
                flagReportDevStatus = 0;
        }

        if (flagFristGetSta && flagReportDevStatus)
        { /*已经获取过主板状态且网络状态发生上线变更*/
            flagReportDevStatus = 0;
            /* 报一次设备状态*/
            LYY_LOGD("report qhxyj change status>>>>>>>>>>>");
            qhhgj_report_change_status(1); // 强制上报
            transmit_cmd_factoryParmGet(); // 获取一次厂家参数
            vTaskDelay(300 / portTICK_PERIOD_MS);
            transmit_cmd_runDataGet(); // 下发洗脱运行数据查询指令(获取一次账目)
        }
        else
        {
            qhhgj_report_change_status(0); // 运行/异常状态/账目变更上报变更时给后台上报
            if (flagfactoryParmGet == 0)
            {

                transmit_cmd_factoryParmGet();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
    }
}

/*获取设备状态倒计时清零，立即触发*/
static void trigger_checkSta_timeout(void) //-- done
{
    if (flagLoopCheckSta)
        timeout_to_checkSta = 0;
    else
        transmit_cmd_bitStatusGet();
}

void device_qhhgj_update_status(void *arg) //-- done
{
    (void)arg;
    if (flagLoopCheckSta)
    {
        if (timeout_to_checkSta)
        {
            timeout_to_checkSta--;
        }
        if (timeout_to_checkSta <= 0)
        {
            transmit_cmd_bitStatusGet();
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            transmit_cmd_runDataGet();
            // LYY_LOGD("timeout_to_checkSta>>>>>>>>>>>>>>>>");
            timeout_to_checkSta = PERIOD_FOR_CHECKSTA / TIME_INTERRUPT_MS;
        }
    }
}
HAL_RTOS_TIMER_T g_hgjTimerHandle;
/**
 * @brief
 *
 */
void lyy_device_qhhgj_init(void) //-- done
{

    HAL_vRtosTimerCreate(&g_hgjTimerHandle,
                         "MS_TIMER",
                         NULL,
                         TIME_INTERRUPT_MS,
                         1,
                         device_qhhgj_update_status);

    xTaskCreate(thread_qhhgj_inquire_status,
                "thread_qhhgj_inquire_status",
                1024 * 5,
                NULL,
                5,
                NULL);
}

/*--------------------------------------------------------------*/
/**
 * @brief
 * 乔禾烘干机设备运行信息上报
 */
void qhhgj_post_dev_status(void) //-- done
{
    cJSON *data = cJSON_CreateObject();
    cJSON *dev_infor = cJSON_CreateObject();
    if (data != NULL && dev_infor != NULL)
    {
        cJSON_AddNumberToObject(data, "run_status", qhghj_rundata.isRunSta);
        // cJSON_AddNumberToObject(data, "mode",qhxyj_washRundata.washNum);
        cJSON_AddNumberToObject(data, "hgj_time_min", qhghj_rundata.time_min);
        cJSON_AddNumberToObject(data, "hgj_time_sec", qhghj_rundata.time_sec);
        cJSON_AddNumberToObject(data, "errvalue", qhghj_rundata.alarmInfo);

        cJSON_AddItemToObject(dev_infor, "dev_status", data);
        thing_property_post(dev_infor);
    }
}

/**
 * @brief
 * 乔禾烘干机设备账目信息事件上报
 */
void qhhgj_event_post_account(void) //-- done
{
    char order[20] = {"88888888888"};
    cJSON *value = cJSON_CreateObject();
    if (value != NULL)
    {
        cJSON_AddNumberToObject(value, "coin_inc", qhhgj_account.cur_coin); // 投币增量
        cJSON_AddNumberToObject(value, "coin_sum", qhhgj_account.coin_all); // 投币总数
        cJSON_AddStringToObject(value, "order", order);                     // 订单号
        cJSON_AddStringToObject(value, "pay_method", "offline_pay");        // 方式
        thing_event_post(value, "accounts");
    }
}

/**
 * @brief
 * 乔禾烘干机设备故障上报
 */
void qhhgj_ctlb_post_up_raw_err(uint8_t errCode) //-- done
{
    uint8_t paramTmp[2];

    paramTmp[0] = 0xFF;
    paramTmp[1] = errCode;

    lyy_thing_ctlb_up_raw(paramTmp, 2);
}
/**
 * @brief
 * 乔禾烘干机设备工厂参数上报
 */
void qhhgj_ctlb_post_up_raw(uint8_t *data, int datalen) //-- done
{
    uint8_t paramTmp[512];
    uint16_t paramLen;
    int i;
    // 组装参数
    memset(paramTmp, 0, sizeof(paramTmp));
    // memcpy(paramTmp+1,data,42*2);

    paramTmp[0] = 0x05;
    // 大小端转换
    for (i = 0; i < datalen / 2; i++)
    {
        paramTmp[1 + 2 * i] = data[2 * i + 1];
        paramTmp[1 + 2 * i + 1] = data[2 * i];
    }
    paramLen = datalen + 1;
    lyy_thing_ctlb_up_raw(paramTmp, paramLen);
}

/**
 * @brief
 * 给后台上报异常状态变更
 * @param package
 * @param len
 */
uint8_t qhhgj_report_change_status(uint8_t force) //-- done
{
    uint8_t ret = 0;
    uint8_t reportFlag = 0;

    uint8_t reportAcountFlag = 0; // 上报账目标志
    // LYY_LOGD("reportAcountFlag>>>>>>>>qhhgjCoinCnt_old:%d qhhgj_account.coin_all：%d",qhhgjCoinCnt_old,qhhgj_account.coin_all);
    if (qhhgjCoinCnt_old != qhhgj_account.coin_all)
    {
        if (force == 1)
        {
            qhhgjCoinCnt_old = qhhgj_account.coin_all; // 第一次强制上报
        }
        reportAcountFlag = 1;
    }

    if (alarmRecord_old != qhhgj_devSta.alarmRecord)
    { /*报警状态变更*/
        // alarmRecord_old = qhhgj_devSta.alarmRecord;
        reportFlag = 1;
    }

    if ((qhhgjRunSta_old != qhhgj_devSta.isRun) || (force == 1))
    { /*运行状态变更*/
        qhhgjRunSta_old = qhhgj_devSta.isRun;
        reportFlag = 1;
        memset(&qhghj_rundata, 0, sizeof(TYPE_REPORT_QHHGJ_STA_INFO)); // 清空待上传数据
        if (qhhgjRunSta_old == 0x01)
        { // 正在运行，获取洗脱机运行数据然后上报
            lyy_semNewV2(&qhhgj_getrunData_sem, 0);
            transmit_cmd_runDataGet();                          // 下发洗脱运行数据查询指令
            ret = lyy_semWaitTryV2(qhhgj_getrunData_sem, 3000); // 等待主板启动应答
            lyy_semFreeV2(qhhgj_getrunData_sem);
            qhhgj_getrunData_sem = NULL;
            if (ret > 0)
            { // 收到主板应答
                LYY_LOGD("hgj runDataGet reply ok");
            }
        }
    }

    if (reportFlag)
    {
        LYY_LOGD("<<<<<<<<<<<<<<<<<Report State change>>>>>>>>>>>>>>>>");
        qhghj_rundata.alarmInfo = qhhgj_devSta.alarmRecord;
        qhghj_rundata.isRunSta = qhhgj_devSta.isRun;
        LYY_LOGD("running data>>>>>>>>HGJ isRunSta:%d", qhghj_rundata.isRunSta);
        LYY_LOGD("running data>>>>>>>>HGJ Time:%d:%d", qhghj_rundata.time_min, qhghj_rundata.time_sec);
        // LYY_LOGD("running data>>>>>>>>HGJ Num:%d",qhghj_rundata.washNum);
        LYY_LOGD("running data>>>>>>>>HGJ  AlarmInfo:%X", qhghj_rundata.alarmInfo);
        device_alarm_info_print();
        if (lyy_mqtt_get_status() == 1)
        { /*网络正常才上报*/
            qhhgj_post_dev_status();
            alarmRecord_old = qhhgj_devSta.alarmRecord;
            device_alarm_info_up_raw();
        }
        /*if(qhhgj_devSta.isRun == 0x01)
        {

            lyy_tjc_page_change_wait();
            LYY_LOGD("lyy_tjc_page_change_wait>>>>>>>>>>>>>>>>");
        }
        else
        {

            lyy_tjc_page_package();
            LYY_LOGD("lyy_tjc_page_package>>>>>>>>>>>>>>>>");
        }*/
    }

    if (reportAcountFlag)
    {
        LYY_LOGD("<<<<<<<<<<<<<<<<<Report accounts change>>>>>>>>>>>>>>>>");
        // if(lyy_mqtt_get_status()==1)
        if ((lyy_mqtt_get_status() == 1) && (qhhgj_devSta.isRun == 1))
        { /*网络正常才上报*/
            if (qhhgj_account.coin_all - qhhgjCoinCnt_old > 0)
            {
                // qhhgj_account.cur_coin = qhhgj_account.coin_all - qhhgjCoinCnt_old;
                // qhhgj_event_post_account();
                if (qhhgjCoinCnt_old == 0) // 第一次上报
                {
                    if (qhhgj_account.cur_coin < 20) // 容错处理
                        qhhgj_event_post_account();
                }
                else
                {
                    qhhgj_account.cur_coin = qhhgj_account.coin_all - qhhgjCoinCnt_old;
                    if (qhhgj_account.cur_coin < 20) // 容错处理
                        qhhgj_event_post_account();
                }
            }

            // LYY_LOGD("running data>>>>>>>>cur_coin:%d",qhhgj_account.cur_coin);
            // LYY_LOGD("running data>>>>>>>>coin_box:%d",qhhgj_account.coin_box);
            // LYY_LOGD("running data>>>>>>>>coin_all:%d",qhhgj_account.coin_all);
            qhhgjCoinCnt_old = qhhgj_account.coin_all;
        }
    }

    return ret;
}

/**
 * @brief
 * 支付申请---乔禾烘干机
 * @param package
 * @param len
 */
int device_qhhgj_pay_apply(void) // done
{
    uint8_t ret;
    LYY_LOGD("device_qhhgj_pay_apply");
    lyy_semNewV2(&qhhgj_checkStaus_sem, 0);
    trigger_checkSta_timeout();                         // transmit_cmd_bitStatusGet( );//给主板发送查询串口指令
    ret = lyy_semWaitTryV2(qhhgj_checkStaus_sem, 3000); // 等待主板启动应答
    lyy_semFreeV2(qhhgj_checkStaus_sem);
    qhhgj_checkStaus_sem = NULL;

    if (ret > 0)
    { // 收到主板应答
        if (qhhgj_devSta.isReady == 0 || qhhgj_devSta.isAlarm == 1)
        { // 上报支付申请异常应答给后台
            LYY_LOGD("qhghj_pay_check is err");
            apply_start_t.apply_result = 0x00; // 0x00-失败 0x01-成功
        }
        else
        { // 上报支付申请正常应答给后台
            LYY_LOGD("qhghj_pay_check is ok");
            apply_start_t.apply_result = 0x01; // 0x00-失败 0x01-成功
        }
    }

    return ret;
}

/**
 * @brief
 * 支付启动--乔禾烘干机
 * @param time
 * @return int
 */
int device_qhhgj_pay_start(uint16_t time)
{
    int repeatCnt = 0;
    uint8_t programNum = time;
    uint16_t coinNun = 0;
    uint8_t ret = 0;

    if (qhhgj_devSta.isRun != 1) // 设备启动中，启动加时，投币数为当前已投币数+支付币数
    {
        if (programNum == 4)
        {
            event_post_log("zhijiejiashi_timeout");
            return 0;
        }
        if (get_dev_type_code() == DEVICE_TYPE_UART_QHHGJ)
        {
            coinNun = qhhgj_devParm.baseCoinCnt;
            if (programNum == 2)
                coinNun += qhhgj_devParm.minAddCoinCnt;
            else if (programNum == 3)
                coinNun += qhhgj_devParm.hightAddCoinCnt;
        }
        else
        {
            LYY_LOGD("programNum[%d]", programNum);
            if (programNum == 1) // 低温
                coinNun = qhhgj_devParm.lowCoinCnt;
            else if (programNum == 2)
                coinNun = qhhgj_devParm.minCoinCnt;
            else if (programNum == 3)
                coinNun = qhhgj_devParm.hightCoinCnt;
            else
                coinNun = qhhgj_devParm.lowCoinCnt;
        }

        /*给主板下发程序选择指令*/
        lyy_semNewV2(&qhhgj_selectProgram_sem, 0);
        transmit_cmd_select_program(programNum);
        /*等待程序选择应答指令*/
        ret = lyy_semWaitTryV2(qhhgj_selectProgram_sem, 3000);
        lyy_semFreeV2(qhhgj_selectProgram_sem);
        qhhgj_selectProgram_sem = NULL;
        if (ret == 0)
        { // 超时未收到主板应答
            event_post_log("transmit_cmd_select_program");
            return 0;
        }
        vTaskDelay(300 / portTICK_PERIOD_MS); // 延时一会
    }
    else
    {
        coinNun = qhhgj_devParm.coinPerPulse;
        //coinNun += qhhgj_account.cur_coin;
    }
    /*给主板下发投币指令*/
    lyy_semNewV2(&qhhgj_sendCoin_sem, 0);
    LYY_LOGD("qhhgj_devParm.lowCoinCnt[%d]", qhhgj_devParm.lowCoinCnt);
    LYY_LOGD("qhhgj_devParm.minCoinCnt[%d]", qhhgj_devParm.minCoinCnt);
    LYY_LOGD("qhhgj_devParm.hightCoinCnt[%d]", qhhgj_devParm.hightCoinCnt);
    LYY_LOGD("transmit_cmd_coin_num >>>>>>>>%d", coinNun);
    if (coinNun > 20)
        return 0; // 币数异常直接不启动 避免引起异常
    transmit_cmd_coin_num(coinNun);
    /*等待启动应答指令*/
    ret = lyy_semWaitTryV2(qhhgj_sendCoin_sem, 3000);
    lyy_semFreeV2(qhhgj_sendCoin_sem);
    qhhgj_sendCoin_sem = NULL;
    if (ret == 0)
    { // 超时未收到主板应答
        event_post_log("transmit_cmd_coin_num");
        return 0;
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);               // 延时一会
    if ((qhhgj_devSta.isRun == 1) || (programNum == 4)) // 设备启动中||加时
    {
        event_post_log("jiashichenggong");
        apply_start_t.start_result = 0x01;
        return 1;
    }
    while (1)
    {
        if (get_dev_type_code() == DEVICE_TYPE_UART_LJHGJ)
        {
            /*给主板下发启动指令*/
            lyy_semNewV2(&qhhgj_start_sem, 0);
            transmit_cmd_start_hgj();
            /*等待启动应答指令*/
            ret = lyy_semWaitTryV2(qhhgj_start_sem, 3000);
            lyy_semFreeV2(qhhgj_start_sem);
            qhhgj_start_sem = NULL;
            if (ret == 0)
            { // 超时未收到主板应答
                event_post_log("LJ_transmit_cmd_start_hgj");
                //return 0;
            }
        }

        vTaskDelay(500 / portTICK_PERIOD_MS); // 延时一会
        lyy_semNewV2(&qhhgj_checkStaus_sem, 0);
        trigger_checkSta_timeout();                         // transmit_cmd_bitStatusGet( );//给主板发送查询串口指令
        ret = lyy_semWaitTryV2(qhhgj_checkStaus_sem, 3000); // 等待主板启动应答
        lyy_semFreeV2(qhhgj_checkStaus_sem);
        qhhgj_checkStaus_sem = NULL;
        if (ret > 0)
        { // 收到主板应答
            // LYY_LOGD("qhhgj_devSta.isRun[%d]", qhhgj_devSta.isRun);
            if (qhhgj_devSta.isRun == 1)
            {
                apply_start_t.start_result = 0x01; // 0x00-失败 0x01-成功
                break;
            }
            else
            {
                apply_start_t.start_result = 0x00; // 0x00-失败 0x01-成功
                event_post_log("apply_start_t.start_result = 0x00");
                lyy_semNewV2(&qhhgj_getrunData_sem, 0);
                transmit_cmd_runDataGet();                          // 下发洗脱运行数据查询指令
                ret = lyy_semWaitTryV2(qhhgj_getrunData_sem, 3000); // 等待主板启动应答
                lyy_semFreeV2(qhhgj_getrunData_sem);
                qhhgj_getrunData_sem = NULL;

                if (qhhgj_account.cur_coin < coinNun)
                {
                    vTaskDelay(2000 / portTICK_PERIOD_MS); // 3秒后重新获取
                    /*给主板下发投币指令*/
                    lyy_semNewV2(&qhhgj_sendCoin_sem, 0);
                    LYY_LOGD("transmit_cmd_coin_num >>>>>>>>%d", coinNun);
                    transmit_cmd_coin_num(coinNun);
                    /*等待启动应答指令*/
                    ret = lyy_semWaitTryV2(qhhgj_sendCoin_sem, 3000);
                    lyy_semFreeV2(qhhgj_sendCoin_sem);
                    qhhgj_sendCoin_sem = NULL;
                }

                repeatCnt++;
            }
        }
        else
        {
            repeatCnt++;
        }
        if (repeatCnt > 3)
        {
            event_post_log("repeatCnt=3");
            break;
        }
    }

    return ret;
}