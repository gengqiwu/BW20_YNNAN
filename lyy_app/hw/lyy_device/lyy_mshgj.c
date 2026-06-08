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
#include "lyy_mshgj.h"
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

static SEM_TYPE mshgj_start_sem;
static SEM_TYPE mshgj_checkStaus_sem;
TYPE_MSHGJ_DEV_STA MSHGJ_devSta = {0};

static int terminal_mshgj_check(void);
static int terminal_mshgj_start_check(void);
/**
 * @brief 
 * 
 * @param arg 
 */
void thread_mshgj_inquire_status(void *arg)
{
    (void)arg;
    uint8_t timeCnt = 0;
    while(1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        if(++timeCnt>=5)
        {

                terminal_mshgj_check();
            
            timeCnt = 0;
        }
        
    } 
}

/**
 * @brief 
 * 
 */
void lyy_device_mshgj_init(void)
{
    terminal_mshgj_start_check();
   /* xTaskCreate(    thread_mshgj_inquire_status, 
                    "thread_mshgj_inquire_status", 
                    1024,     //20250610 ZYL jianxiaoneicui gai  4->2
                    NULL, 
                    5,
                    NULL); 
   */
}

static int terminal_mshgj_start_check(void)
{
    uint8_t txbuff[12] = {0};

    txbuff[0] = 0x53;//帧头
    txbuff[1] = 0x4A;//帧头
    txbuff[2] = 0x0C;//数据长度
    txbuff[3] = 0x04;//厂家ID,暂时固定为0X04
    txbuff[10] = 0X00;//工作状态查询
    txbuff[11] = check_xor_memory((const uint8_t *)txbuff, 11);//BCC异或校验码             /*异常标记*/

    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}


/*******************************************************************************
*函数名称：int terminalCWSendCHK()
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：下发自检查询
*******************************************************************************/
static int terminal_mshgj_check(void)
{
    uint8_t txbuff[12] = {0};

    txbuff[0] = 0x53;//帧头
    txbuff[1] = 0x4A;//帧头
    txbuff[2] = 0x0C;//数据长度
    txbuff[3] = 0x04;//厂家ID,暂时固定为0X04
    txbuff[10] = 0X23;//工作状态查询
    txbuff[11] = check_xor_memory((const uint8_t *)txbuff, 11);//BCC异或校验码             /*异常标记*/

    //memset(&MSHGJ_devSta,0,sizeof(TYPE_MSHGJ_DEV_STA));
    MSHGJ_devSta.devSta=0;
    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}


static int terminal_mshgj_check_result(void)
{
    uint8_t txbuff[12] = {0};

    txbuff[0] = 0x53;//帧头
    txbuff[1] = 0x4A;//帧头
    txbuff[2] = 0x0C;//数据长度
    txbuff[3] = 0x04;//厂家ID,暂时固定为0X04
    txbuff[10] = 0X09;//
    txbuff[11] = check_xor_memory((const uint8_t *)txbuff, 11);//BCC异或校验码             /*异常标记*/

    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}

/*******************************************************************************
*函数名称：
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：下发启动
*******************************************************************************/
static int terminal_mshgj_start(uint8_t mode)
{
    uint8_t txbuff[12] = {0};

    txbuff[0] = 0x53;//帧头
    txbuff[1] = 0x4A;//帧头
    txbuff[2] = 0x0C;//数据长度
    txbuff[3] = 0x04;//厂家ID,暂时固定为0X04
    txbuff[10] = mode+2;//洗衣机模式
    txbuff[11] = check_xor_memory((const uint8_t *)txbuff, 11);//BCC异或校验码             /*异常标记*/

    //memset(&MSHGJ_devSta,0,sizeof(TYPE_MSHGJ_DEV_STA));
    MSHGJ_devSta.runAnswer=0;
    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}

static int terminal_mshgj_start_result(void)
{
    uint8_t txbuff[12] = {0};

    txbuff[0] = 0x53;//帧头
    txbuff[1] = 0x4A;//帧头
    txbuff[2] = 0x0C;//数据长度
    txbuff[3] = 0x04;//厂家ID,暂时固定为0X04
    txbuff[10] = 0x01;//洗
    txbuff[11] = check_xor_memory((const uint8_t *)txbuff, 11);//BCC异或校验码             /*异常标记*/

    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}

/**
 * @brief
 * 支付申请---乔禾洗衣机
 * @param package
 * @param len
 */
int mshgj_pay_apply(void)
{
    uint8_t ret=0;
    LYY_LOGD("mshgj_pay_apply");   
    lyy_semNewV2(&mshgj_checkStaus_sem, 0); 
    terminal_mshgj_check();//给主板发送查询串口指令
    ret = lyy_semWaitTryV2(mshgj_checkStaus_sem, 3000); //等待主板启动应答
    lyy_semFreeV2(mshgj_checkStaus_sem);
    mshgj_checkStaus_sem = NULL;
    if(ret>0)
    {//收到主板应答    0x01 工作   0x02 空闲
        if(MSHGJ_devSta.devSta == 0x02)  
        {
            //上报支付申请正常应答给后台
            LYY_LOGD("mshgj_pay_check is ok");
            terminal_mshgj_check_result();
            apply_start_t.apply_result = 0x01;//0x00-失败 0x01-成功
        }
        else
        {
            //上报支付申请异常应答给后台
            LYY_LOGD("mshgj_pay_check is err");
            apply_start_t.apply_result = 0x00;//0x00-失败 0x01-成功
        }
    }

    return ret;
}

/**
 * @brief 
 * 支付启动--麦穗烘干机
 * @param time 
 * @return int 
 */
int mshgj_pay_start(uint16_t mode)
{
    uint16_t repeatCnt = 0;
    uint8_t ret = 0;
    while(1)
    {
        /*给主板下发启动指令*/
        lyy_semNewV2(&mshgj_start_sem, 0);
        terminal_mshgj_start(mode);
        /*等待启动应答指令*/
        ret = lyy_semWaitTryV2(mshgj_start_sem, 3000);
        lyy_semFreeV2(mshgj_start_sem);
        mshgj_start_sem = NULL;
        if(ret>0)
        {//收到主板应答   
            if(MSHGJ_devSta.runAnswer==0X01){
                terminal_mshgj_start_result();
                apply_start_t.start_result = 0x01;//0x00-失败 0x01-成功
                break;
            }
            else{ 
                apply_start_t.start_result = 0x00;//0x00-失败 0x01-成功
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
 * 
 * @param package
 * @param len
 */
void mshgj_packet_parse(uint8_t *package, uint16_t len)
{
    LYY_LOGD("mshgj_packet_parse\r\n");
    lyy_printf_hex(package,len);
    if((package[0]==0x53) &&(package[1]==0x4A)&& (len==package[2]))
    {//烘干机协议指令
        if(package[10] == 0x00 )//上电应答
        {

        }
        else if(package[10] == 0x01 )//启动指令应答
        {

             //package[5] 设备状态 0x00空闲 0x01工作中 0x02故障mshgj_checkStaus_sem
             //MSHGJ_devSta.devSta = package[5];
             MSHGJ_devSta.runAnswer = package[10];
             if(mshgj_start_sem != NULL){
                    lyy_semSignalV2(mshgj_start_sem);//接收到应答
                    LYY_LOGD("mshgj_start_sem\r\n");
            }
        }
        else if(package[10] == 0x02 )//烘干完成信号
        {
            MSHGJ_devSta.devSta=0x02; //0x02空闲状态
           LYY_LOGD("xyj_start_sem\r\n");
        }
        else if(package[10] == 0x09 )//工作状态告知
        {
           MSHGJ_devSta.devSta = package[11];
           if(mshgj_checkStaus_sem != NULL){
                lyy_semSignalV2(mshgj_checkStaus_sem);//接收到应答
            }
           LYY_LOGD("xyj_start_sem\r\n");
        }
        else if(package[10] == 0x24 )//故障代码告知
        {
           
        }
    }

}








