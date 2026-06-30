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
#include "lyy_lgxyjmax.h"
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

#include "thing_iot.h"
#include "thing_mqtt.h"
//#include "sys_packet.h"
//#include "os_inter.h"
#include "lyy_para.h"
//#include "bsp_osInter.h"

#define LG13KG_WSM_JYJ_TASK_MSEC    (500U)

static void lg13kg_wsm_jyj_device_poweron(void);
static void lg13kg_wsm_jyj_device_netMonitor(void);
static void terminal_lg13kg_wsm_jyj_startup_loop(void);

typedef struct
{
    uint8_t status;           // 运行状态   0x01待机，此状态可启动; 0x02启动中; 0x03工作中；0x04暂停中；0x05故障
    uint8_t remain_timeH;     // 剩余时间(小时)
    uint8_t remain_timeM;     // 剩余时间(分)
    uint8_t auto_reprot_flag; // 烘干机自动上报状态标志
} lg13kg_wsm_jyj_device_t;

typedef struct
{
    uint8_t jyj_mode;     // 是否有加液标志
    uint8_t mode;         // 模式
    uint8_t preWash;      // 是否投入预洗液
    uint8_t mainWash;     // 是否投入主洗液
    uint8_t bleach;       // 是否投入消毒液
    uint8_t softner;      // 是否投入柔顺液
    uint8_t emulsifier;   // 是否投入乳化液
    int timeout;          // 超时计时
    uint8_t step_flag;    // 启动标志
    uint8_t get_jyj_flag; // 获取到加液指令标志
    uint8_t result;       // 启动结果
} lg13g_wsm_jyj_startup_t;

static lg13g_wsm_jyj_startup_t lg13kg_wsm_jyj_startup = {0};
static lg13kg_wsm_jyj_device_t lg13kg_wsm_jyj_info = {0};



/*******************************************************************************
*函数名称：terminal_lg13kg_wsm_jyj_startup
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：LG滚筒洗衣机启动指令（可控制LG加液机）
*******************************************************************************/
static void terminal_lg13kg_wsm_jyj_startup(uint8_t mode,uint8_t preWash,uint8_t mainWash,uint8_t bleach,uint8_t softner,uint8_t emulsifier)
{
	uint16_t checksum = 0;
	uint8_t sendBuff[15]={0xAA ,0x0F ,0xF0 ,0x24 ,0x16 ,0x00 ,0x00 ,0x05 ,0x09 ,0x00 ,0x00 ,0x64 ,0x00 ,0x00 ,0xBB};
	switch(mode)
	{
		case 1:sendBuff[8]=0x08;sendBuff[9]=0x00;break;//深度桶自洁
		case 2:sendBuff[8]=0x09;sendBuff[9]=0x00;break;//热水洗
		case 3:sendBuff[8]=0x0A;sendBuff[9]=0x00;break;//温水洗
		case 4:sendBuff[8]=0x0B;sendBuff[9]=0x00;break;//冷水洗
		case 5:sendBuff[8]=0x0C;sendBuff[9]=0x00;break;//精致衣物洗
		case 6:sendBuff[8]=0x0D;sendBuff[9]=0x00;break;//单脱水
		case 7:sendBuff[8]=0x0E;sendBuff[9]=0x00;break;//3分钟桶自洁
		case 8:sendBuff[8]=0x09;sendBuff[9]=0x01;break;//热水加强洗
		case 9:sendBuff[8]=0x0A;sendBuff[9]=0x01;break;//温水加强洗
		case 10:sendBuff[8]=0x0B;sendBuff[9]=0x01;break;//冷水加强洗
		case 11:sendBuff[8]=0x0C;sendBuff[9]=0x01;break;//精致衣物加强洗
	}
	if(preWash)
	{
		sendBuff[12] |= (1<<0);
	}
	if(mainWash)
	{
		sendBuff[12] |= (1<<1);
	}
	if(bleach)
	{
		sendBuff[12] |= (1<<2);
	}
	if(softner)
	{
		sendBuff[12] |= (1<<3);
	}
	if(emulsifier)
	{
		sendBuff[12] |= (1<<4);
	}

	if(sendBuff[12]>0)
	{
		sendBuff[9] |= 0x02;//使用自动加液功能
	}
	//保持第1~12个数据的累加和为427，因此这样才能保证倒数第二个数（校验值）是0x00
	for(int i=1;i<=12;i++)
	{
		if(i!=11)
		{
			checksum += sendBuff[i];
		}
	}
	sendBuff[11] = 427 - checksum;

	// memset(LG_NEW_START_SEND,0,sizeof(LG_NEW_START_SEND));
	// memcpy(LG_NEW_START_SEND,sendBuff,sizeof(sendBuff));//保存启动指令，用于重发

	terminal_device_data( (uint8_t *)sendBuff , sizeof(sendBuff) );
}

/*******************************************************************************
*函数名称：terminal_lg13kg_wsm_jyj_stop
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：LG滚筒洗衣机停止指令（可控制LG加液机）
*******************************************************************************/
static void terminal_lg13kg_wsm_jyj_stop(void)
{
	uint8_t sendBuff[10]={0xAA ,0x0A ,0xF0 ,0x24 ,0x08 ,0x00 ,0x00 ,0x00 ,0x85 ,0xBB};
	terminal_device_data( (uint8_t *)sendBuff , sizeof(sendBuff) );
}

//启动结果上报02
static void lg13kg_wsm_jyj_startup_netty_report(uint8_t status,uint8_t time_h,uint8_t time_m)
{
	uint8_t buf[8] = {0xBB,0x08,0x02,0x00,0x00,0x00,0x00,0xDD};
    uint16_t TIME = time_h*60 + time_m;
	buf[3] = status;
	buf[4] = (TIME&0xFF00)>>8;
    buf[5] = (TIME&0x00FF)>>0;
	buf[6] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3]^ buf[4]^ buf[5];
    //xlogWriteHex("lg13kg_wsm_jyj_report:",(uint8_t *)buf, sizeof(buf));
    //lyy_netty_terminal_dtu_packet(buf, sizeof(buf));

    return;
}
/*******************************************************************************
*函数名称：lg13kg_wsm_jyj_status_netty_report
*函数说明：
*输入参数：
*输出参数：
*返 回 值：
*其他说明：LG滚筒洗衣机状态上报指令（可控制LG加液机）
*******************************************************************************/
static void lg13kg_wsm_jyj_status_netty_report(uint8_t status,uint8_t time_h,uint8_t time_m)
{
	uint8_t buf[8] = {0xBB,0x08,0x01,0x00,0x00,0x00,0x00,0xDD};
    uint16_t TIME = time_h*60 + time_m;
	buf[3] = status;
	buf[4] = (TIME&0xFF00)>>8;
    buf[5] = (TIME&0x00FF)>>0;
	buf[6] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3]^ buf[4]^ buf[5];
	//xlogWriteHex("lg13kg_wsm_jyj_report:",(uint8_t *)buf, sizeof(buf));
    //lyy_netty_terminal_dtu_packet( buf, sizeof(buf)); 
    return;
}

//加液指令接收结果上报03
static void terminal_lg13kg_wsm_jyj_startjyj_report(uint8_t result)
{
    uint8_t buf[6] = {0xBB,0x06,0x03,0x00,0x00,0xDD};
    buf[3] = result;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3] ;
    //xlogWriteHex("lg13kg_wsm_jyj_report:",(uint8_t *)buf, sizeof(buf));
	//lyy_netty_terminal_dtu_packet( buf, sizeof(buf)); 
}

//远程停止上报04
static void terminal_lg13kg_wsm_jyj_stop_report(void)
{
	uint8_t buf[6] = {0xBB,0x06,0x04,0x00,0x00,0xDD};
    buf[3] = 0x01;
	buf[4] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3];
	//xlogWriteHex("lg13kg_wsm_jyj_report:",(uint8_t *)buf, sizeof(buf));
	//lyy_netty_terminal_dtu_packet( buf, sizeof(buf)); 
}
//剩余时间查询上报05
static void terminal_lg13kg_wsm_jyj_remain_time_report(uint8_t remain_timeH,uint8_t remain_timeM)
{
	uint8_t buf[7] = {0xBB,0x07,0x05,0x00,0x00,0x00,0xDD};
    uint16_t TIME = remain_timeH*60 + remain_timeM;
    buf[3] = (TIME&0xFF00)>>8;
    buf[4] = (TIME&0x00FF)>>0;
	buf[5] = buf[0] ^ buf[1] ^ buf[2] ^ buf[3] ^ buf[4];
    //xlogWriteHex("lg13kg_wsm_jyj_report:",(uint8_t *)buf, sizeof(buf));
	//lyy_netty_terminal_dtu_packet( buf, sizeof(buf)); 
}


//netty数据处理
int lg13kg_wsm_jyj_netty_server_data_handle(uint8_t *pack,uint16_t len)
{
    //这是自定义的后台与设备的协议
    //xlogWriteHex("lg13kg_wsm_jyj_netty:",(uint8_t *)pack, len);
    if( (pack[0] == 0xAA) && (pack[len - 1] == 0xDD) && (pack[1] == 0x06 ) &&  (pack[2] == 0x01) )
    {//状态查询01
        lg13kg_wsm_jyj_status_netty_report(lg13kg_wsm_jyj_info.status,lg13kg_wsm_jyj_info.remain_timeH,lg13kg_wsm_jyj_info.remain_timeM);
    }
    else if( (pack[0] == 0xAA) && (pack[len - 1] == 0xDD) && (pack[1] == 0x06 ) &&  (pack[2] == 0x02) )
    {//启动02
        if(lg13kg_wsm_jyj_info.status==0x01)
        {
            memset(&lg13kg_wsm_jyj_startup,0,sizeof(lg13g_wsm_jyj_startup_t));
            lg13kg_wsm_jyj_startup.mode = pack[3];//模式
            lg13kg_wsm_jyj_startup.jyj_mode = 1;//是否需要加液，有则延时等待加液指令后再启动
            lg13kg_wsm_jyj_startup.step_flag = 1;
            lg13kg_wsm_jyj_startup.get_jyj_flag = 0;
            lg13kg_wsm_jyj_startup.timeout = 0;
            //如果有加液，需要先上报启动成功
            lg13kg_wsm_jyj_startup_netty_report(0x01,0,0);
        }
        else
        {//非待机状态
            //上报不可启动
            lg13kg_wsm_jyj_startup_netty_report(lg13kg_wsm_jyj_info.status,
                                            lg13kg_wsm_jyj_info.remain_timeH,
                                            lg13kg_wsm_jyj_info.remain_timeM);
        }
        
    }
    else if( (pack[0] == 0xAA) && (pack[len - 1] == 0xDD) && (pack[1] == 0x0A ) &&  (pack[2] == 0x03) )
    {//下发加液指令03
        lg13kg_wsm_jyj_startup.preWash = pack[3];
        lg13kg_wsm_jyj_startup.mainWash = pack[4];
        lg13kg_wsm_jyj_startup.bleach = pack[5];
        lg13kg_wsm_jyj_startup.softner = pack[6];
        lg13kg_wsm_jyj_startup.emulsifier = pack[7];
        lg13kg_wsm_jyj_startup.get_jyj_flag = 1;
        terminal_lg13kg_wsm_jyj_startjyj_report(0x01);
    }
    else if( (pack[0] == 0xAA) && (pack[len - 1] == 0xDD) && (pack[1] == 0x06 ) &&  (pack[2] == 0x04) )
    {//结束洗衣04
        terminal_lg13kg_wsm_jyj_stop( );
        terminal_lg13kg_wsm_jyj_stop_report();
    }
    else if( (pack[0] == 0xAA) && (pack[len - 1] == 0xDD) && (pack[1] == 6 ) &&  (pack[2] == 0x05) )
    {//查询剩余时间05
        if(lg13kg_wsm_jyj_startup.step_flag==0)//启动过程中查询时间不返回
        {
            terminal_lg13kg_wsm_jyj_remain_time_report(lg13kg_wsm_jyj_info.remain_timeH,
                                                    lg13kg_wsm_jyj_info.remain_timeM);
        }
        
    }

    return 1;
}


int lg13kg_wsm_jyj_device_uart_data_handle(uint8_t *pack, uint16_t len)
{
    (void)len;
    if (pack[1] == 0x61 && pack[2] == 0x71 && pack[3] == 0xDE)
    { // 状态回复
        /*
        pack[12]表示当前运行的程序对应“Hot”"Cold" "Warm" "Delicate"...，其中00表示没有运行的程序
        pack[13]表示状态，01表示停止，06表示运行中，0A表示正在停止
        pack[14]可能表示故障状态）
        pack[15]可能表示运行剩余时间(小时)）
        pack[16]可能表示运行剩余时间(分钟)）
        */
        lg13kg_wsm_jyj_info.remain_timeH = pack[15];
        lg13kg_wsm_jyj_info.remain_timeM = pack[16];
        if (pack[12] == 0x00 && pack[13] == 0x01 && pack[14] == 0x00)
        { // 正常待机状态
            lg13kg_wsm_jyj_info.status = 0x01;
        }
        else if (pack[14] != 0x00)
        { // 故障
            lg13kg_wsm_jyj_info.status = 0x05;
        }
        else
        { // 运行中
            lg13kg_wsm_jyj_info.status = 0x03;
        }
    }
    else if (pack[1] == 0x08 && pack[2] == 0x71 && pack[3] == 0x00 && pack[4] == 0x24 && pack[5] == 0x00 && pack[6] == 0x12)
    { // 启动/停止 指令回复
    }
    else if (pack[1] == 0x08 && pack[2] == 0x71 && pack[3] == 0x00 && pack[4] == 0x21 && pack[5] == 0x00 && pack[6] == 0x11)
    { // 开启主板主动上报状态指令的回复
        lg13kg_wsm_jyj_info.auto_reprot_flag = 1;
    }

    return 1;
}

static void lg13kg_wsm_jyj_device_task(void *arg)
{
    (void)arg;
    int time_cnt = 0;
    while (1)
    {
        if (time_cnt % 500 == 0) // 500ms
        {
            lg13kg_wsm_jyj_device_poweron();
        }

        if (time_cnt % 1000 == 0) // 1S
        {
        }

        if (time_cnt % 5000 == 0) // 5S
        {
            lg13kg_wsm_jyj_device_netMonitor();
            time_cnt = 0;
        }

        terminal_lg13kg_wsm_jyj_startup_loop(); // 启动流程

        vTaskDelay(LG13KG_WSM_JYJ_TASK_MSEC / portTICK_PERIOD_MS);
        time_cnt += 500;
    }
}

void lg13kg_wsm_jyj_device_task_init(void)
{
    xTaskCreate(lg13kg_wsm_jyj_device_task,
                "lg13kg_wsm_jyj_device_task",
                1024 * 4, // 
                NULL,
                5,
                NULL);
}

// 连接维护
static void lg13kg_wsm_jyj_device_poweron(void)
{
    // uint8_t sendBuff[8]={0xAA ,0x08 ,0xF0 ,0x00 ,0xDE ,0x04 ,0xD1 ,0xBB};
    // uint8_t sendBuff1[6]={0xAA ,0x06 ,0xF0 ,0xF9 ,0xCC ,0xBB};
    // uint8_t sendBuff2[6]={0xAA ,0x06 ,0xF0 ,0xFF ,0xCA ,0xBB};
    // uint8_t sendBuff3[8]={0xAA ,0x08 ,0xE0 ,0x15 ,0x01 ,0x22 ,0x9F ,0xBB};
    // uint8_t sendBuff4[7]={0xAA ,0x07 ,0xE0 ,0x29 ,0x01 ,0xEE ,0xBB};
    // uint8_t sendBuff5[23]={0xAA ,0x17 ,0xE0 ,0xFC ,0x63 ,0x6C ,0x69 ,0x70 ,0x5F ,0x68 ,0x6E ,0x61 ,0x5F ,0x76 ,0x31 ,0x2E ,0x39 ,0x2E ,0x32 ,0x31 ,0x30 ,0x5C ,0xBB};
    // uint8_t sendBuff6[6]={0xAA ,0x06 ,0xF0 ,0x32 ,0x87 ,0xBB};
    // uint8_t sendBuff7[6]={0xAA ,0x06 ,0xF0 ,0x21 ,0x94 ,0xBB};
    static uint8_t step = 1;
    static uint8_t cnt = 0;
    uint8_t buff[25] = {0};
    switch (step)
    {
    case 1:
    { // 0xAA ,0x06 ,0xF0 ,0xF9 ,0xCC ,0xBB
        buff[0] = 0xAA;
        buff[1] = 0x06;
        buff[2] = 0xF0;
        buff[3] = 0xF9;
        buff[4] = 0xCC;
        buff[5] = 0xBB;
        terminal_device_data((uint8_t *)buff, 6);
        step++;
    }
    break;
    case 2:
    { // 0xAA ,0x06 ,0xF0 ,0xFF ,0xCA ,0xBB
        buff[0] = 0xAA;
        buff[1] = 0x06;
        buff[2] = 0xF0;
        buff[3] = 0xFF;
        buff[4] = 0xCA;
        buff[5] = 0xBB;
        terminal_device_data((uint8_t *)buff, 6);
        step++;
    }
    break;
    case 3:
    { // 0xAA ,0x08 ,0xE0 ,0x15 ,0x01 ,0x22 ,0x9F ,0xBB
        buff[0] = 0xAA;
        buff[1] = 0x08;
        buff[2] = 0xE0;
        buff[3] = 0x15;
        buff[4] = 0x01;
        buff[5] = 0x22;
        buff[6] = 0x9F;
        buff[7] = 0xBB;
        terminal_device_data((uint8_t *)buff, 8);
        step++;
    }
    break;
    case 4:
    { // 0xAA ,0x07 ,0xE0 ,0x29 ,0x01 ,0xEE ,0xBB
        buff[0] = 0xAA;
        buff[1] = 0x07;
        buff[2] = 0xE0;
        buff[3] = 0x29;
        buff[4] = 0x01;
        buff[5] = 0xEE;
        buff[6] = 0xBB;
        terminal_device_data((uint8_t *)buff, 7);
        step++;
    }
    break;
    case 5:
    { // 0xAA ,0x17 ,0xE0 ,0xFC ,0x63 ,0x6C ,0x69 ,0x70 ,0x5F ,0x68 ,0x6E ,0x61 ,0x5F ,0x76 ,0x31 ,0x2E ,0x39 ,0x2E ,0x32 ,0x31 ,0x30 ,0x5C ,0xBB
        buff[0] = 0xAA;
        buff[1] = 0x17;
        buff[2] = 0xE0;
        buff[3] = 0xFC;
        buff[4] = 0x63;
        buff[5] = 0x6C;
        buff[6] = 0x69;
        buff[7] = 0x70;
        buff[8] = 0x5F;
        buff[9] = 0x68;
        buff[10] = 0x6E;
        buff[11] = 0x61;
        buff[12] = 0x5F;
        buff[13] = 0x76;
        buff[14] = 0x31;
        buff[15] = 0x2E;
        buff[16] = 0x39;
        buff[17] = 0x2E;
        buff[18] = 0x32;
        buff[19] = 0x31;
        buff[20] = 0x30;
        buff[21] = 0x5C;
        buff[22] = 0xBB;
        terminal_device_data((uint8_t *)buff, 23);
        step++;
    }
    break;
    case 6:
    { // 0xAA ,0x06 ,0xF0 ,0x32 ,0x87 ,0xBB
        buff[0] = 0xAA;
        buff[1] = 0x06;
        buff[2] = 0xF0;
        buff[3] = 0x32;
        buff[4] = 0x87;
        buff[5] = 0xBB;
        terminal_device_data((uint8_t *)buff, 6);
        step++;
    }
    break;
    case 7:
    { // 0xAA ,0x06 ,0xF0 ,0x21 ,0x94 ,0xBB(触发主板定时主动上报状态)
        buff[0] = 0xAA;
        buff[1] = 0x06;
        buff[2] = 0xF0;
        buff[3] = 0x21;
        buff[4] = 0x94;
        buff[5] = 0xBB;
        terminal_device_data((uint8_t *)buff, 6);
        if (lg13kg_wsm_jyj_info.auto_reprot_flag)
        {
            step++;
        }
    }
    break;
    }
    if (step > 7)
    {
        cnt = (cnt + 1) % 2;
        if (cnt == 0)
        { // 0xAA ,0x08 ,0xF0 ,0x00 ,0xDE ,0x04 ,0xD1 ,0xBB
            buff[0] = 0xAA;
            buff[1] = 0x08;
            buff[2] = 0xF0;
            buff[3] = 0x00;
            buff[4] = 0xDE;
            buff[5] = 0x04;
            buff[6] = 0xD1;
            buff[7] = 0xBB;
            terminal_device_data((uint8_t *)buff, 8);
        }
    }
}

// 网络监控
static void lg13kg_wsm_jyj_device_netMonitor(void)
{
    uint8_t sendBuff1[8] = {0xAA, 0x08, 0xF0, 0x00, 0xFF, 0x00, 0xF4, 0xBB}; // 没网络时
    uint8_t sendBuff2[8] = {0xAA, 0x08, 0xF0, 0x00, 0xFF, 0x04, 0xF0, 0xBB};

    if (1) // lyy_server_business_connectSta_get() >= LYY_NETTY_LOGIN_OK
    {
        terminal_device_data((uint8_t *)sendBuff2, sizeof(sendBuff2));
    }
    else
    {
        terminal_device_data((uint8_t *)sendBuff1, sizeof(sendBuff1));
    }
}

static void terminal_lg13kg_wsm_jyj_startup_loop(void)
{
    if (lg13kg_wsm_jyj_startup.step_flag == 0) // 无启动
        return;

    switch (lg13kg_wsm_jyj_startup.step_flag)
    {
    case 1:
    { // 收到启动指令，判断是否有加液
        LYY_LOGD("(1/5)   lg13kg_wsm_jyj_startup.jyj_mode[%d]", lg13kg_wsm_jyj_startup.jyj_mode);
        if (lg13kg_wsm_jyj_startup.jyj_mode == 0)
        { // 没有加液，直接启动
            lg13kg_wsm_jyj_startup.step_flag = 3;
        }
        else
        { // 有加液，等待加液指令
            lg13kg_wsm_jyj_startup.step_flag = 2;
            lg13kg_wsm_jyj_startup.timeout = 0;
        }
    }
    break;
    case 2:
    { // 等待加液指令
        LYY_LOGD("(2/5)   lg13kg_wsm_jyj_startup.timeout[%d]", lg13kg_wsm_jyj_startup.timeout);

        if (lg13kg_wsm_jyj_startup.get_jyj_flag)
        {                                         // 获取到加液指令
            lg13kg_wsm_jyj_startup.step_flag = 3; // 下一步启动
        }

        lg13kg_wsm_jyj_startup.timeout += LG13KG_WSM_JYJ_TASK_MSEC;
        if (lg13kg_wsm_jyj_startup.timeout >= 5000) // 5秒无应答
        {
            lg13kg_wsm_jyj_startup.step_flag = 3; // 下一步启动
        }
    }
    break;
    case 3:
    { // 下发启动指令给主板
        LYY_LOGD("(3/5)   lg13kg_wsm_jyj_startup.mode[%d]", lg13kg_wsm_jyj_startup.mode);
        terminal_lg13kg_wsm_jyj_startup(lg13kg_wsm_jyj_startup.mode,
                                        lg13kg_wsm_jyj_startup.preWash, lg13kg_wsm_jyj_startup.mainWash,
                                        lg13kg_wsm_jyj_startup.bleach, lg13kg_wsm_jyj_startup.softner, lg13kg_wsm_jyj_startup.emulsifier);
        lg13kg_wsm_jyj_startup.step_flag = 4; // 下一步
        lg13kg_wsm_jyj_startup.timeout = 0;
    }
    break;
    case 4:
    { // 等待启动应答
        LYY_LOGD("(4/5)lg13kg_wsm_jyj startup wait[%d]", lg13kg_wsm_jyj_startup.timeout);
        if ((lg13kg_wsm_jyj_info.status == 0x03) && (lg13kg_wsm_jyj_info.remain_timeM != 0 || lg13kg_wsm_jyj_info.remain_timeH != 0))
        {
            lg13kg_wsm_jyj_startup.result = 0x01; // 启动成功
            lg13kg_wsm_jyj_startup.step_flag = 5; // 上报结果
        }

        lg13kg_wsm_jyj_startup.timeout += LG13KG_WSM_JYJ_TASK_MSEC;
        if (lg13kg_wsm_jyj_startup.timeout >= 3000) // 三秒无应答
        {
            lg13kg_wsm_jyj_startup.result = 0x00; // 启动失败
            lg13kg_wsm_jyj_startup.step_flag = 5; // 上报结果
        }
    }
    break;
    case 5:
    { // 上报启动结果
        LYY_LOGD("(5/5)lg13kg_wsm_jyj startup finish");
        lg13kg_wsm_jyj_startup_netty_report(lg13kg_wsm_jyj_startup.result, lg13kg_wsm_jyj_info.remain_timeH, lg13kg_wsm_jyj_info.remain_timeM);
        lg13kg_wsm_jyj_startup.step_flag = 0;
    }
    break;
    }
    return;
}
