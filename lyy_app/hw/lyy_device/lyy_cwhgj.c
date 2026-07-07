/*
 * @author: e.he
 * @Date: 2024-07-10 16:29:48
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-10-09 16:31:10
 * @Description: 
 * 
 */
#include "lyy_xyj.h"
#include "lyy_cwhgj.h"
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

#include "thing_iot.h"
#include "thing_mqtt.h"
//#include "sys_packet.h"
//#include "os_inter.h"
#include "lyy_para.h"
//#include "bsp_osInter.h"
#include "utils.h"

static SEM_TYPE hgj_start_sem;
static SEM_TYPE hgj_checkStaus_sem;
TYPE_SKYWORTH_HGJ_DEV_STA skyworth_hgj_devSta = {0};

static int terminal_skyworthhgj_check(void);
static uint8_t fCWCheckOk = 0;//开机后自检完成标志


/**
 * @brief 
 * 
 * @param arg 
 */
void thread_cwhgj_inquire_status(void *arg)
{
    (void)arg;
    uint8_t timeCnt = 0;
    while(1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if(++timeCnt>=5)
        {
            if(fCWCheckOk == 0)
            {
                terminal_skyworthhgj_check();
            }
            
            timeCnt = 0;
        }
        
    } 
}

/**
 * @brief 
 * 
 */
void lyy_device_cwhgj_init(void)
{

    xTaskCreate(thread_cwhgj_inquire_status,
                "thread_cwhgj_inquire_status",
                1024 * 2, // 20250610 ZYL jianxiaoneicui gai  4->2
                NULL,
                5,
                NULL);
}



/*******************************************************************************
*函数名称：int terminalCWSendCHK()
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：创维10kg滚筒洗衣机-下发自检查询
*******************************************************************************/
static int terminal_skyworthhgj_check(void)
{
	uint8_t txbuff[10] = {0xAA, 0x00, 0x01, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0xDD};
	uint8_t devid_string[20]= {0};
	char *dev_id = APP_pcGetParamDevId();    /*异常标记*/
	uint8_t i, XORdata = 0;
	

    utils_lhexToString((char *)devid_string, (const unsigned char *)dev_id,8);
	txbuff[5] = devid_string[13];
	txbuff[6] = devid_string[14];
	txbuff[7] = devid_string[15];

	for(i = 0; i < 8; i++)
	{
		XORdata ^= txbuff[i];
	}
	txbuff[8] = XORdata;

    memset(&skyworth_hgj_devSta,0,sizeof(TYPE_SKYWORTH_HGJ_DEV_STA));
    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}

/*******************************************************************************
*函数名称：int terminalCWSendCHK()
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：创维10kg滚筒洗衣机-下发启动
*******************************************************************************/
static int terminal_skyworthhgj_start(uint8_t mode)
{
    uint8_t txbuff[18] = {0};

    txbuff[0] = 0xAA;//帧头
    txbuff[1] = 0x00;//源地址
    txbuff[2] = 0x01;//目标地址
    txbuff[3] = 0x07;//功能码
    txbuff[4] = 0x0B;//数据长度
    txbuff[5] = 0x01;//启动设备
    txbuff[6] = mode;//烘干机模式
    txbuff[7] = 0x00; // UV灯
    txbuff[8] = 0x02;// 桶内温度
    txbuff[16] = check_xor_memory((const uint8_t *)txbuff, 16);//BCC异或校验码             /*异常标记*/
    txbuff[17] = 0xDD;//帧尾

    memset(&skyworth_hgj_devSta,0,sizeof(TYPE_SKYWORTH_HGJ_DEV_STA));
    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}


/**
 * @brief
 * 支付申请---乔禾洗衣机
 * @param package
 * @param len
 */
int skyworth_qhhgj_pay_apply(void)
{
    uint8_t ret=0;
    LYY_LOGD("skyworth_qhhgj_pay_apply");   
    lyy_semNewV2(&hgj_checkStaus_sem, 0); 
    terminal_skyworthhgj_check();//给主板发送查询串口指令
    ret = lyy_semWaitTryV2(hgj_checkStaus_sem, 3000); // 等待主板启动应答
    lyy_semFreeV2(hgj_checkStaus_sem);
    hgj_checkStaus_sem = NULL;

    if(ret>0)
    {//收到主板应答
        if(skyworth_hgj_devSta.devSta == 0x01)
        {
            //上报支付申请正常应答给后台
            LYY_LOGD("qhxyj_pay_check is ok");
            apply_start_t.apply_result = 0x01;//0x00-失败 0x01-成功
        }
        else if(skyworth_hgj_devSta.devSta == 0x02)
        {
            //上报支付申请异常应答给后台
            LYY_LOGD("qhxyj_pay_check is err");
            apply_start_t.apply_result = 0x00;//0x00-失败 0x01-成功
        }
        else{
            // 上报支付申请异常应答给后台
            LYY_LOGD("qhxyj_pay_check is other");
            apply_start_t.apply_result = 0x11; // 0X11 烘干中  0x12烘干结束
        }
    }

    return ret;
}

/**
 * @brief 
 * 支付启动--乔禾洗衣机
 * @param time 
 * @return int 
 */
int skyworth_hgj_pay_start(uint16_t mode)
{
    uint16_t repeatCnt = 0;
    uint8_t ret = 0;
    while(1)
    {
        /*给主板下发启动指令*/
        lyy_semNewV2(&hgj_start_sem, 0);
        terminal_skyworthhgj_start(mode);
        /*等待启动应答指令*/
        ret = lyy_semWaitTryV2(hgj_start_sem, 3000);
        lyy_semFreeV2(hgj_start_sem);
        hgj_start_sem = NULL;
        if(ret>0)
        {//收到主板应答
            if(skyworth_hgj_devSta.devSta==0x11){
                apply_start_t.start_result = 0x01;//
                break;
            }
            else if (skyworth_hgj_devSta.devSta == 0x01)
            {
                apply_start_t.start_result = 0x00; // 
                break;
            }
            else if (skyworth_hgj_devSta.devSta == 0x03)
            {
                apply_start_t.start_result = 0x02; // 
                break;
            }
            else{ 
                apply_start_t.start_result = 0x0A;// 其他
                break;
            }
        }
        else
        {
            repeatCnt++;
        }
        if(repeatCnt>3)
        {
            break;
        }
    }
    return ret;
}



/**
 * @brief
 * 创维滚筒洗衣机串口数据帧处理方法
 * @param package
 * @param len
 */
void skyworth_hgj_packet_parse(uint8_t *package, uint16_t len)
{
     lyy_printf_hex(package,len);
     
    LYY_LOGD("skyworth_xyj_packet_parse\r\n");
    lyy_printf_hex(package,len);
    if((package[0]==0xAA) && (package[len-1]==0xDD) && (len==package[4]+7) )
    {//洗衣机协议指令
        if(package[3] == 0x06 )//查询指令应答
        {
            //package[5] 设备状态 0x00空闲 0x01工作中 0x02故障
            //package[8] 洗衣机故障码
            skyworth_hgj_devSta.devSta = package[5];
            skyworth_hgj_devSta.devErrCode = package[7];
            if(hgj_checkStaus_sem != NULL){
                    lyy_semSignalV2(hgj_checkStaus_sem);//接收到应答
                    LYY_LOGD("hgj_checkStaus_sem\r\n");
            }
            if(fCWCheckOk==0)
            {
                fCWCheckOk = 1;
            }
        }
        else if(package[3] == 0x07 )//启动指令应答
        {
             //package[5] 设备状态 0x01启动失败 0x02空闲 0x03故障  0x11烘干中 0x12烘干结束
             skyworth_hgj_devSta.devSta = package[5];
             if(hgj_start_sem != NULL){
                    lyy_semSignalV2(hgj_start_sem);//接收到应答
                    LYY_LOGD("hgj_start_sem\r\n");
            }
        }
    }

}








