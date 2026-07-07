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
#include "lyy_tclxyj.h"
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

static SEM_TYPE tclxyj_apply_sem;
static SEM_TYPE tclxyj_start_sem;

uint16_t time_remaining = 0;   //机器运行的剩余时间

/*--------------------------------------------------------------------------------------------------
                                  对返回的数据进行校验的算法
--------------------------------------------------------------------------------------------------*/
// 对接收的数据的正确性进行校验，如果返回值是true，代表数据正确，如果返回值是false，代表数据有误
uint8_t check_crc(uint8_t *buf, uint16_t len)
{
    //机器返回的数据总是以0x02  0x70开头，并且在倒数第二位的数据固定为0x03，如果接收到的数据没有这些，就代表其数据
    if (!((buf[0] == 0x02) && (buf[1] == 0x70) && (buf[2] == 0x74) && (buf[len - 2] == 0x03)))
        return 0;
    uint8_t i = 0;
    uint16_t j = 0;
    uint8_t lrc = 0;
    uint8_t temp_buf[100] = {0};
    uint8_t ret = 0;
    for (i = 0; i < len - 2; i++)
        temp_buf[i] = *(buf + 1 + i);
    for (i = 0; i < 7; i++)
    {
        uint8_t data = 0;
        for (j = 0; j < len - 2; j++)
            data = ((temp_buf[j] >> i) & 0x01) + data;
        lrc = (lrc | ((data & 0x01) << i));
    }
    ret = (lrc & 0x7F);
    if(ret == buf[len - 1])
        return 1;
    else
        return 0;
}

uint8_t tcl_crc(uint8_t *buf, uint16_t len) // 整个数据，长度包含CRC校验位   比如整个查询时间指令 02 74 70 62 03 65  输入长度len为6
{
    uint8_t i = 0;
    uint16_t j = 0;
    uint8_t lrc = 0;
    uint8_t temp_buf[100] = {0};
    uint8_t ret = 0;
    for (i = 0; i < len - 2; i++)
        temp_buf[i] = *(buf + 1 + i);
    for (i = 0; i < 7; i++)
    {
        uint8_t data = 0;
        for (j = 0; j < len - 2; j++)
            data = ((temp_buf[j] >> i) & 0x01) + data;
        lrc = (lrc | ((data & 0x01) << i));
    }
    ret = (lrc & 0x7F);
    return ret;
}



void tclxyi_check_time_cmd(void)
{
    unsigned char remain_time_query[] = {0x02,  /* 帧头，固定0x02 */
                                               0x74,  /* 源地址，0x74在协议中表示模块 */
                                               0x70,  /* 目的地址，0x70在协议中表示洗衣机 */
                                               0x62,  /* 0x62对应剩余时间查询和门状态 */
                                               0x03,  /* 帧尾 */
                                               0x65}; /* 校验和 */

    terminal_device_data(remain_time_query, sizeof(remain_time_query));
}

void tclxyi_check_state_cmd(void)    //该指令机器为启动时，没有作用，所以暂不使用
{
    unsigned char state_query[] = {0x02,  /* 帧头，固定0x02 */
                                         0x74,  /* 源地址，0x74在协议中表示模块 */
                                         0x70,  /* 目的地址，0x70在协议中表示洗衣机 */
                                         0x63,  /* 0x63对应查询指令 */
                                         0x03,  /* 固定帧尾 */
                                         0x64}; /* 校验和 */

    terminal_device_data(state_query, sizeof(state_query));
}

void tclxyi_kuai_su_xi_cmd(void)
{
    unsigned char kuai_su_xi[] = {0x02,                                                                               /* 帧头，固定0x02 */
                                        0x74,                                                                               /* 源地址，0x74在协议中表示模块 */
                                        0x70,                                                                               /* 目的地址，0x70在协议中表示洗衣机 */
                                        0x61, 0x00,                                                                         /* 0x61 0x00表示这是一个启动指令 */
                                        0x33, 0x30, 0x33, 0x31,                                                             /* 这四个字节对应快速洗 */
                                        0x33, 0x30, 0x33, 0x30, 0x33, 0x36, 0x33, 0x34, 0x33, 0x30, 0x33, 0x32, 0x33, 0x30, /* 这块涉及到转速，温度之类的，不用管 */
                                        0x03,                                                                               /* 帧尾，固定0x03 */
                                        0x64};                                                                              /* 校验和 */

    terminal_device_data(kuai_su_xi, sizeof(kuai_su_xi));
}

void tclxyi_biao_zhun_xi_cmd(void)
{
    unsigned char biao_zhun_xi[] = {0x02,                                                                               /* 帧头，固定0x02 */
                                          0x74,                                                                               /* 源地址，0x74在协议中表示模块 */
                                          0x70,                                                                               /* 目的地址，0x70在协议中表示洗衣机 */
                                          0x61, 0x00,                                                                         /* 0x61 0x00表示这是一个启动指令 */
                                          0x33, 0x30, 0x33, 0x32,                                                             /* 这四个字节对应标准洗 */
                                          0x33, 0x30, 0x33, 0x30, 0x33, 0x36, 0x33, 0x34, 0x33, 0x30, 0x33, 0x32, 0x33, 0x30, /* 这块涉及到转速，温度之类的，不用管 */
                                          0x03,                                                                               /* 帧尾，固定0x03 */
                                          0x67};                                                                              /* 校验和 */
    terminal_device_data(biao_zhun_xi, sizeof(biao_zhun_xi));
}

void tclxyi_da_wu_xi_cmd(void)
{
    unsigned char da_wu_xi[] = {0x02,                                                                               /* 帧头，固定0x02 */
                                      0x74,                                                                               /* 源地址，0x74在协议中表示模块 */
                                      0x70,                                                                               /* 目的地址，0x70在协议中表示洗衣机 */
                                      0x61, 0x00,                                                                         /* 0x61 0x00表示这是一个启动指令 */
                                      0x33, 0x30, 0x33, 0x33,                                                             /* 这四个字节对应大物洗 */
                                      0x33, 0x30, 0x33, 0x30, 0x33, 0x36, 0x33, 0x34, 0x33, 0x30, 0x33, 0x32, 0x33, 0x30, /* 这块涉及到转速，温度之类的，不用管 */
                                      0x03,                                                                               /* 帧尾，固定0x03 */
                                      0x66};                                                                              /* 校验和 */
    terminal_device_data(da_wu_xi, sizeof(da_wu_xi));
}

void tclxyi_dan_tuo_shui_cmd(void)
{
    unsigned char dan_tuo_shui[] = {0x02,                                                                               /*帧头，固定0x02*/
                                          0x74,                                                                               /*源地址，0x74在协议中表示模块*/
                                          0x70,                                                                               /*目的地址，0x70在协议中表示洗衣机*/
                                          0x61, 0x00,                                                                         /*0x61 0x00表示这是一个启动指令*/
                                          0x33, 0x30, 0x33, 0x35,                                                             /* 这四个字节对应单脱水 */
                                          0x33, 0x30, 0x33, 0x30, 0x33, 0x36, 0x33, 0x34, 0x33, 0x30, 0x33, 0x32, 0x33, 0x30, /* 这块涉及到转速，温度之类的，不用管 */
                                          0x03,                                                                               /*帧尾，固定0x03*/
                                          0x60};                                                                              /*校验和*/
    terminal_device_data(dan_tuo_shui, sizeof(dan_tuo_shui));
}

void tclxyi_tong_zi_jie_cmd(void)
{
    unsigned char tong_zi_jie[] = {0x02,                                                                               /* 帧头，固定0x02 */
                                         0x74,                                                                               /* 源地址，0x74在协议中表示模块 */
                                         0x70,                                                                               /* 目的地址，0x70在协议中表示洗衣机 */
                                         0x61, 0x00,                                                                         /* 0x61 0x00表示这是一个启动指令 */
                                         0x33, 0x30, 0x33, 0x36,                                                             /* 这四个字节对应桶自洁 */
                                         0x33, 0x30, 0x33, 0x30, 0x33, 0x36, 0x33, 0x34, 0x33, 0x30, 0x33, 0x32, 0x33, 0x30, /* 这块涉及到转速，温度之类的，不用管 */
                                         0x03,                                                                               /* 帧尾，固定0x03 */
                                         0x63};                                                                              /* 校验和 */

    terminal_device_data(tong_zi_jie, sizeof(tong_zi_jie));
}

void tclxyi_stop_cmd(void)
{
    // 执行停止指令，立即恢复到空闲状态
    // 返回的指令示例
    //   串口助手发送  02 74 70 73 33 30 33 31 03 75
    //   串口助手返回  02 70 74 37 33 33 30 33 31 03 02
    unsigned char stop[] = {0x02,                   /* 帧头，固定0x02 */
                                  0x74,                   /* 源地址，0x74在协议中表示模块 */
                                  0x70,                   /* 目的地址，0x70在协议中表示洗衣机 */
                                  0x73,                   /* 这个字节代表停止指令 */
                                  0x33, 0x30, 0x33, 0x31, /* 停止 */
                                  0x03,                   /* 帧尾 */
                                  0x75};                  /* 帧尾 */

    terminal_device_data(stop, sizeof(stop));
}

uint8_t asc_to_time_remaining(uint8_t *data)   //计算剩余时间时，转换使用
{
    if (data[0] == 0x33 && data[1] == 0x30 )
    {
        return 0;
    }
    else if (data[0] == 0x33 && data[1] == 0x31)
    {
        return 1;
    }
    else if (data[0] == 0x33 && data[1] == 0x32)
    {
        return 2;
    }
    else if (data[0] == 0x33 && data[1] == 0x33)
    {
        return 3;
    }
    else if (data[0] == 0x33 && data[1] == 0x34)
    {
        return 4;
    }
    else if (data[0] == 0x33 && data[1] == 0x35)
    {
        return 5;
    }
    else if (data[0] == 0x33 && data[1] == 0x36)
    {
        return 6;
    }
    else if (data[0] == 0x33 && data[1] == 0x37)
    {
        return 7;
    }
    else if (data[0] == 0x33 && data[1] == 0x38)
    {
        return 8;
    }
    else if (data[0] == 0x33 && data[1] == 0x39)
    {
        return 9;
    }
    else if (data[0] == 0x36 && data[1] == 0x31)
    {
        return 10;
    }
    else if (data[0] == 0x36 && data[1] == 0x32)
    {
        return 11;
    }
    else if (data[0] == 0x36 && data[1] == 0x33)
    {
        return 12;
    }
    else if (data[0] == 0x36 && data[1] == 0x34)
    {
        return 13;
    }
    else if (data[0] == 0x36 && data[1] == 0x35)
    {
        return 14;
    }
    else if (data[0] == 0x36 && data[1] == 0x36)
    {
        return 15;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief
 * 支付申请---TCL洗衣机
 * @param package
 * @param len
 */
int device_tclxyj_pay_apply(void)
{
    uint8_t ret = 0;
    LYY_LOGD("device_tclxyj_pay_apply");
    lyy_semNewV2(&tclxyj_apply_sem, 0);
    tclxyi_check_time_cmd();                             // 给主板发送查询剩余时间指令
    ret = lyy_semWaitTryV2(tclxyj_apply_sem, 3000);     // 等待主板启动应答
    lyy_semFreeV2(tclxyj_apply_sem);
    tclxyj_apply_sem = NULL;
    if (ret > 0)
    { // 收到主板应答    0x01 工作   0x02 空闲
        if (time_remaining >= 1)
        {
            LYY_LOGD("device_tclxyj_pay_apply is err");
            apply_start_t.apply_result = 0x00; //
        }
        else
        {
            LYY_LOGD("device_tclxyj_pay_apply is ok");
            apply_start_t.apply_result = 0x01; //
        }
    }

    return ret;
}

/**
 * @brief
 * 支付启动---TCL洗衣机
 * @param package
 * @param len
 */
int device_tclxyj_pay_start(uint16_t mode)
{
    uint16_t repeatCnt = 0;
    uint8_t ret = 0;
    while (1)
    {
        /*给主板下发启动指令*/
        lyy_semNewV2(&tclxyj_start_sem, 0);
        if (mode==1)
        {
            tclxyi_kuai_su_xi_cmd();
        }
        else if (mode == 2)
        {
            tclxyi_biao_zhun_xi_cmd();
        }
        else if (mode == 3)
        {
            tclxyi_da_wu_xi_cmd();
        }
        else if (mode == 4)
        {
            tclxyi_dan_tuo_shui_cmd();
        }
        else if (mode == 5)
        {
            tclxyi_tong_zi_jie_cmd();
        }
            /*等待启动应答指令*/
        ret = lyy_semWaitTryV2(tclxyj_start_sem, 3000);
        lyy_semFreeV2(tclxyj_start_sem);
        tclxyj_start_sem = NULL;
        if (ret > 0)
        { // 收到主板应答
            apply_start_t.start_result = 0x01; // 
            break;
        }
        else
        {
            repeatCnt++;
        }
        if (repeatCnt > 3)
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
void tclxyj_packet_parse(uint8_t *package, uint16_t len)
{
    LYY_LOGD("tclxyj_packet_parse\r\n");
    lyy_printf_hex(package, len);

    if (check_crc(package, len) != 1)
        return;

    if (package[3] == 0x36 && package[4] == 0x32) // 查询时间的应答
    {
        time_remaining = asc_to_time_remaining(&package[5]) * 4096 + asc_to_time_remaining(&package[7]) * 256 + asc_to_time_remaining(&package[9]) * 16 + asc_to_time_remaining(&package[11]);

        if (tclxyj_apply_sem != NULL)
        {
            lyy_semSignalV2(tclxyj_apply_sem); // 接收到应答
        }
    }
    else if (package[3] == 0x36 && package[4] == 0x33) //  状态查询的应答
    {
    }
    else if (package[3] == 0x36 && package[4] == 0x31) //  启动成功的应答
    {
        if (tclxyj_start_sem != NULL)
        {
            lyy_semSignalV2(tclxyj_start_sem); // 接收到应答
        }
    }
    else if (package[3] == 0x37 && package[4] == 0x33) //  停止的应答
    {
    }
}
