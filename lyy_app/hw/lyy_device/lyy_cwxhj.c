/*
 * @author: e.he
 * @Date: 2024-07-10 16:29:48
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-10-09 16:31:10
 * @Description: 
 * 
 */
#include "lyy_xyj.h"
#include "lyy_cwxhj.h"
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

static SEM_TYPE xhj_start_sem;
static SEM_TYPE xhj_checkStaus_sem;
TYPE_SKYWORTH_XHJ_DEV_STA skyworth_xhj_devSta = {0};

static int terminal_skyworthxhj_check(void);
static uint8_t fCWCheckOk = 0;//开机后自检完成标志


/**
 * @brief 
 * 
 * @param arg 
 */
void thread_cwxhj_inquire_status(void *arg)
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
                terminal_skyworthxhj_check();
            }
            
            timeCnt = 0;
        }
        
    } 
}

/**
 * @brief 
 * 
 */
void lyy_device_cwxhj_init(void)
{

    xTaskCreate(thread_cwxhj_inquire_status,
                "thread_cwxhj_inquire_status",
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
*其他说明：
*******************************************************************************/
static int terminal_skyworthxhj_check(void)
{
	uint8_t txbuff[7] = {0xAA, 0x00, 0x01, 0x06, 0x00, 0xAD};

    memset(&skyworth_xhj_devSta,0,sizeof(TYPE_SKYWORTH_XHJ_DEV_STA));
    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}

/*******************************************************************************
*函数名称：int terminalCWSendCHK()
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：txbuff[5]洗衣模式  筒自洁（冷水）：0x01  筒自洁（95°）：0x02  单脱水：0x03  快洗：0x04 标准洗：0x05
                            大物洗：0x06  加温洗：0x07  快洗烘：0x08   冷风烘干：0x09 低温烘干：0x0a 中温烘干：0x0b 高温烘干：0x0c 能效洗衣：0x0d
          txbuff[6]烘干时长   快洗烘：
启动筒自洁：AA 00 01 07 03 01 01 03 AC
启动高温筒自洁：AA 00 01 07 03 01 02 82 2E
启动单脱水：AA 00 01 07 03 01 03 09 A4
启动快速洗：AA 00 01 07 03 01 04 19 B3
启动标准洗：AA 00 01 07 03 01 05 23 88
启动大物洗：AA 00 01 07 03 01 06 32 9A
启动加温洗：AA 00 01 07 03 01 07 37 9E
启动快洗烘：AA 00 01 07 03 01 08 2D 8B
启动冷风烘干：AA 00 01 07 03 01 09 23 84
启动低温烘干：AA 00 01 07 03 01 0A 32 96
启动中温烘干：AA 00 01 07 03 01 0B 3C 99
启动高温烘干：AA 00 01 07 03 01 0C 3C 9E
启动能效洗衣：AA 00 01 07 03 01 0D 3B 98
启动极速洗烘：AA 00 01 07 03 01 10 3C 82
启动标准洗烘：AA 00 01 07 03 01 20 5A D4
启动强力洗烘：AA 00 01 07 03 01 30 78 E6
启动超净洗烘：AA 00 01 07 03 01 40 96 78
停止工作：AA 00 01 07 03 04 01 00 AA

*******************************************************************************/
static int terminal_skyworthxhj_start(uint8_t mode)
{
    uint8_t txbuff[9] = {0};

    txbuff[0] = 0xAA;//帧头
    txbuff[1] = 0x00;//源地址
    txbuff[2] = 0x01;//目标地址
    txbuff[3] = 0x07;//功能码
    txbuff[4] = 0x03;//数据长度
    txbuff[5] = 0x01; // 0x01：启动设备   0x02：启动设备功能累加   0x04:复位洗衣机

    if (mode ==1)  //快洗
    {
        txbuff[6] = 0x04;
        txbuff[7] = 0x19;
    }
    else if (mode == 2)  //标准洗
    {
        txbuff[6] = 0x05;
        txbuff[7] = 0x23;
    }
    else if (mode == 3)  //大物洗
    {
        txbuff[6] = 0x06;
        txbuff[7] = 0x32;
    }
    else if (mode == 4)  //快洗烘
    {
        txbuff[6] = 0x08;
        txbuff[7] = 0x2D;
    }
    else if (mode == 5)  //中温烘干
    {
        txbuff[6] = 0x0b;
        txbuff[7] = 0x3c;
    }
    else if (mode == 6)  //高温烘干
    {
        txbuff[6] = 0x0c;
        txbuff[7] = 0x3c;
    }
    else if (mode == 7)   //筒自洁
    {
        //待填写
        txbuff[6] = 0x01;
        txbuff[7] = 0x03;
    }
    else
    {
        txbuff[6] = 0x00;
        txbuff[7] = 0x00;
    }

    txbuff[8] = check_xor_memory((const uint8_t *)txbuff, 8);//BCC异或校验码     

    memset(&skyworth_xhj_devSta,0,sizeof(TYPE_SKYWORTH_XHJ_DEV_STA));
    return terminal_device_data((uint8_t*)txbuff,sizeof(txbuff));
}


/**
 * @brief
 * 支付申请---乔禾洗衣机
 * @param package
 * @param len
 */
int skyworth_qhxhj_pay_apply(void)
{
    uint8_t ret=0;
    LYY_LOGD("skyworth_qhxhj_pay_apply");   
    lyy_semNewV2(&xhj_checkStaus_sem, 0); 
    terminal_skyworthxhj_check();//给主板发送查询串口指令
    ret = lyy_semWaitTryV2(xhj_checkStaus_sem, 3000); // 等待主板启动应答
    lyy_semFreeV2(xhj_checkStaus_sem);
    xhj_checkStaus_sem = NULL;

    if(ret>0)
    {//收到主板应答
        if(skyworth_xhj_devSta.devSta == 0x01)
        {
            //上报支付申请正常应答给后台
            LYY_LOGD("qhxhj_pay_check is ok");
            apply_start_t.apply_result = 0x01;//0x00-失败 0x01-成功
        }
        else{
            // 上报支付申请异常应答给后台
            LYY_LOGD("qhxhj_pay_check is fail");
            apply_start_t.apply_result = 0x00; //
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
int skyworth_xhj_pay_start(uint16_t mode)
{
    uint16_t repeatCnt = 0;
    uint8_t ret = 0;
    while(1)
    {
        /*给主板下发启动指令*/
        lyy_semNewV2(&xhj_start_sem, 0);
        terminal_skyworthxhj_start(mode);
        /*等待启动应答指令*/
        ret = lyy_semWaitTryV2(xhj_start_sem, 3000);
        lyy_semFreeV2(xhj_start_sem);
        xhj_start_sem = NULL;
        if(ret>0)
        {//收到主板应答
            if(skyworth_xhj_devSta.devSta==0x01){
                apply_start_t.start_result = 0x01;//
                break;
            }
            else{ 
                apply_start_t.start_result = 0x00;// 其他
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
void skyworth_xhj_packet_parse(uint8_t *package, uint16_t len)
{
     lyy_printf_hex(package,len);
     
    LYY_LOGD("skyworth_xhj_packet_parse\r\n");
    lyy_printf_hex(package,len);
    if((package[0]==0xAA)  && (len==package[4]+6) )
    {//洗衣机协议指令
        if(package[3] == 0x06 )//查询指令应答
        {
            if (package[5] == 0x00)
            {
                skyworth_xhj_devSta.devSta = 1;
            }else{
                skyworth_xhj_devSta.devSta = 0;
            }
            
            skyworth_xhj_devSta.devErrCode = package[8];  //故障码，待使用
            if(xhj_checkStaus_sem != NULL){
                    lyy_semSignalV2(xhj_checkStaus_sem);//接收到应答
                    LYY_LOGD("xhj_checkStaus_sem\r\n");
            }
            if(fCWCheckOk==0)
            {
                fCWCheckOk = 1;
            }
        }
        else if(package[3] == 0x07 )//启动指令应答
        {
            if (package[5] == 0x0F)
            {
                skyworth_xhj_devSta.devSta = 1;
            }
            else
            {
                skyworth_xhj_devSta.devSta = 0;
            }
             if(xhj_start_sem != NULL){
                    lyy_semSignalV2(xhj_start_sem);//接收到应答
                    LYY_LOGD("xhj_start_sem\r\n");
            }
        }
    }

}








