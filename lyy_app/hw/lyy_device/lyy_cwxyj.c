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
#include "lyy_cwxyj.h"
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

static SEM_TYPE xyj_start_sem;
static SEM_TYPE xyj_checkStaus_sem;
static SEM_TYPE CYJYJ_start_sem;
TYPE_SKYWORTH_XYJ_DEV_STA skyworth_xyj_devSta = {0};

static int terminal_skyworthXYJ_check(void);
static uint8_t fCWCheckOk = 0;//开机后自检完成标志


/**
 * @brief 
 * 
 * @param arg 
 */
void thread_cwxyj_inquire_status(void *arg)
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
                terminal_skyworthXYJ_check();
            }
            
            timeCnt = 0;
        }
        
    } 
}

HAL_TIME_HANDLE_T g_cwxyjTimerHandle;
/**
 * @brief 
 * 
 */
void lyy_device_cwxyj_init(void)
{

    xTaskCreate(    thread_cwxyj_inquire_status, 
                    "thread_cwxyj_inquire_status", 
                    1024*2,     //20250610 ZYL jianxiaoneicui gai  4->2
                    NULL, 
                    5,
                    NULL); 
}

/**
 * @brief  停止功能
 *
 */
int terminal_skyworthXYJ_stop(void)
{
    uint8_t txbuff[7] = {0xAA, 0x00, 0x01, 0x08, 0x00, 0xA3, 0xDD};

    return terminal_device_data((uint8_t *)txbuff, sizeof(txbuff));
}

/*******************************************************************************
*函数名称：int terminalCWSendCHK()
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：创维10kg滚筒洗衣机-下发自检查询
*******************************************************************************/
static int terminal_skyworthXYJ_check(void)
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

    memset(&skyworth_xyj_devSta,0,sizeof(TYPE_SKYWORTH_XYJ_DEV_STA));
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
static int terminal_skyworthXYJ_start(uint8_t mode)
{
    uint8_t txbuff[18] = {0};

    txbuff[0] = 0xAA;//帧头
    txbuff[1] = 0x00;//源地址
    txbuff[2] = 0x01;//目标地址
    txbuff[3] = 0x07;//功能码
    txbuff[4] = 0x0B;//数据长度
    txbuff[5] = 0x00;//启动设备
    txbuff[6] = mode;//洗衣机模式
    txbuff[16] = check_xor_memory((const uint8_t *)txbuff, 16);//BCC异或校验码             /*异常标记*/
    txbuff[17] = 0xDD;//帧尾

    memset(&skyworth_xyj_devSta,0,sizeof(TYPE_SKYWORTH_XYJ_DEV_STA));
    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}


/**
 * @brief
 * 支付申请---乔禾洗衣机
 * @param package
 * @param len
 */
int skyworth_qhxyj_pay_apply(void)
{
    uint8_t ret=0;
    LYY_LOGD("skyworth_qhxyj_pay_apply");   
    lyy_semNewV2(&xyj_checkStaus_sem, 0); 
    terminal_skyworthXYJ_check();//给主板发送查询串口指令
    ret = lyy_semWaitTryV2(xyj_checkStaus_sem, 3000); //等待主板启动应答
    lyy_semFreeV2(xyj_checkStaus_sem);
    xyj_checkStaus_sem = NULL;

    if(ret>0)
    {//收到主板应答
        if(skyworth_xyj_devSta.devSta == 0x00)
        {
            //上报支付申请正常应答给后台
            LYY_LOGD("qhxyj_pay_check is ok");
            apply_start_t.apply_result = 0x01;//0x00-失败 0x01-成功
        }
        else
        {
            //上报支付申请异常应答给后台
            LYY_LOGD("qhxyj_pay_check is err");
            apply_start_t.apply_result = 0x00;//0x00-失败 0x01-成功
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
int skyworth_xyj_pay_start(uint16_t mode)
{
    uint16_t repeatCnt = 0;
    uint8_t ret = 0;
    while(1)
    {
        /*给主板下发启动指令*/
        lyy_semNewV2(&xyj_start_sem, 0);
        terminal_skyworthXYJ_start(mode);
        /*等待启动应答指令*/
        ret = lyy_semWaitTryV2(xyj_start_sem, 3000);
        lyy_semFreeV2(xyj_start_sem);
        xyj_start_sem = NULL;
        if(ret>0)
        {//收到主板应答
            if(skyworth_xyj_devSta.devSta==1){
                apply_start_t.start_result = 0x01;//0x00-失败 0x01-成功
                break;
            }
            else{ 
                apply_start_t.start_result = 0x00;//0x00-失败 0x01-成功
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
/*
**************************************************************************************************************
* 函数名称:     InvertUint8
* 函数功能:     将uint8类型的数字进行bit全部反转
* 函数入口:     dBuf    [out]   待反转的值
                srcBuf  [in]    反转后的值
* 函数返回:        计算CRC的结果
***************************************************************************************************************/

static void InvertUint8(unsigned char *dBuf,unsigned char *srcBuf)
{
    int i;
    unsigned char tmp[4];
    tmp[0] = 0;
    for(i=0;i< 8;i++)
    {
        if(srcBuf[0]& (1 << i))
        tmp[0]|=1<<(7-i);
    }
    dBuf[0] = tmp[0];
}
/*
**************************************************************************************************************
* 函数名称:     InvertUint16
* 函数功能:     将uint16类型的数字进行bit全部反转
* 函数入口:     dBuf    [out]   待反转的值
                srcBuf  [in]    反转后的值
* 函数返回:        计算CRC的结果
***************************************************************************************************************/
static void InvertUint16(unsigned short *dBuf,unsigned short *srcBuf)
{
    int i;
    unsigned short tmp[4];
    tmp[0] = 0;
    for(i=0;i< 16;i++)
    {
        if(srcBuf[0]& (1 << i))
            tmp[0]|=1<<(15 - i);
    }
    dBuf[0] = tmp[0];
}
/*
**************************************************************************************************************
* 函数名称:     CY_CRC16
* 函数功能:     计算CRC16
* 函数入口:     puchMsg    [in]    待计算的数据
               usDataLen  [in]    待计算数据的长度
* 函数返回:     计算CRC的结果
***************************************************************************************************************/
static unsigned short CY_CRC16(unsigned char *puchMsg, unsigned int usDataLen)
{
    unsigned short wCRCin = 0xFFFF;        //初始值，根据模式进行设置
    unsigned short wCPoly = 0x7006;        //多项式，根据模式进行设置7006/1021
    unsigned char wChar = 0;

    while (usDataLen--)
    {
        wChar = *(puchMsg++);
        
        InvertUint8(&wChar,&wChar);     //输入数据反转，根据模式进行选择是否启用
        
        wCRCin ^= (wChar << 8);
        for(int i = 0;i < 8;i++)
        {
            if(wCRCin & 0x8000)
                wCRCin = (wCRCin << 1) ^ wCPoly;
            else
                wCRCin = wCRCin << 1;
        }
    }
    
    InvertUint16(&wCRCin,&wCRCin);        //输出数据反转，根据模式进行选择是否启用
    
    wCRCin &= 0xFFFF;
    
    wCRCin ^= 0x0000;                    //输出异或，根据模式设置异或的数值
    
    return (wCRCin) ;
}


static uint8_t cyjyj_start_result = 0;
/*******************************************************************************
*函数名称：uint8_t CYJYJ_updateDataChk(uint8_t *pTemp, uint16_t len)
*函数说明：
*输入参数：
*输出参数：
*返 回 值：  1：是采亿加液机数据    0：其他数据
*其他说明：
*******************************************************************************/
uint8_t CYJYJ_dataChk(uint8_t *pTemp, uint16_t len)
{
	uint8_t ret = 0;
	uint16_t cycrc  = 0;

	//if((pTemp[0] == 0x13) && (pTemp[1] == 0x03) && (pTemp[2] == 0x1d)) //采亿加液机数据长度有bug，暂不判断这个字节
	if((pTemp[0] == 0x13) && (pTemp[2] == 0x1d)) //启动应答
	{	
		cycrc = CY_CRC16(pTemp, len-2);
		if(((cycrc >> 8) == pTemp[len-2]) && ((cycrc & 0xff) == pTemp[len-1]))
		{
            LYY_LOGD(">>>>>>>>>>>>>>>>>>>>>>> CY JYJ start reply!");
            if(pTemp[3] == 1)
            {//加液机启动成功应答
                cyjyj_start_result = 1;
            }
            else
            {//加液机启动失败应答
                cyjyj_start_result = 0;
            }
            if(CYJYJ_start_sem != NULL){
                    lyy_semSignalV2(CYJYJ_start_sem);//接收到应答
                    LYY_LOGD("CYJYJ_start_sem\r\n");
            }
			ret = 1;
		}
		else
        {
            ret = 0;
        }
			
	}
	return ret;
}


uint8_t CYJYJ_start(uint8_t data1, uint8_t data2)
{
    uint8_t ret=0;
    uint16_t cycrc  = 0;
    uint8_t txBuf[13]={0};
    txBuf[0]=0x13;
    txBuf[1]=0x09;
    txBuf[2]=0x1d;
    if(data1>0)
    {
        txBuf[3]=0x01;
        txBuf[4]=data1;//加液1时间
    }
    if(data2>0)
    {
        txBuf[5]=0x01;
        txBuf[6]=data2;//加液2时间
    }
    cycrc = CY_CRC16(txBuf, 11);
    txBuf[11] = ((cycrc&0xFF00)>>8);
    txBuf[12] = (cycrc&0xFF);
    
    for(int i=0;i<3;i++)
    {
        lyy_semNewV2(&CYJYJ_start_sem, 0); 
        terminal_device_data((uint8_t*)txBuf,sizeof(txBuf));//发送串口协议指令
        ret = lyy_semWaitTryV2(CYJYJ_start_sem, 3000); //等待主板启动应答
        lyy_semFreeV2(CYJYJ_start_sem);
        CYJYJ_start_sem = NULL;

        if(ret>0)
        {//收到主板应答
            if(cyjyj_start_result)
            {
                return 1;
            }
            else
            {
                //重发
            }
        }
    }
    
    return 0;
}


/**
 * @brief
 * 创维滚筒洗衣机串口数据帧处理方法
 * @param package
 * @param len
 */
void skyworth_xyj_packet_parse(uint8_t *package, uint16_t len)
{
     lyy_printf_hex(package,len);
     
    if(CYJYJ_dataChk(package, len))
    {//加液机协议指令
        return;
    }
    LYY_LOGD("skyworth_xyj_packet_parse\r\n");
    lyy_printf_hex(package,len);
    if((package[0]==0xAA) && (package[len-1]==0xDD) && (len==package[4]+7) )
    {//洗衣机协议指令
        if(package[3] == 0x06 )//查询指令应答
        {
            //package[5] 设备状态 0x00空闲 0x01工作中 0x02故障
            //package[8] 洗衣机故障码
            skyworth_xyj_devSta.devSta = package[5];
            skyworth_xyj_devSta.devErrCode = package[8];
            if(xyj_checkStaus_sem != NULL){
                    lyy_semSignalV2(xyj_checkStaus_sem);//接收到应答
                    LYY_LOGD("xyj_checkStaus_sem\r\n");
            }
            if(fCWCheckOk==0)
            {
                fCWCheckOk = 1;
            }
        }
        else if(package[3] == 0x07 )//启动指令应答
        {
             //package[5] 设备状态 0x00空闲 0x01工作中 0x02故障
             skyworth_xyj_devSta.devSta = package[5];
             if(xyj_start_sem != NULL){
                    lyy_semSignalV2(xyj_start_sem);//接收到应答
                    LYY_LOGD("xyj_start_sem\r\n");
            }
        }
    }

}








