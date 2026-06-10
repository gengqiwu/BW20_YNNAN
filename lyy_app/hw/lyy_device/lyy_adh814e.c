/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:51
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 16:46:18
 * @Description: 
 */
#include <stdio.h>
#include <string.h>


#include "APP_Param.h"
#include "APP_DrillingMode.h"

#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_WIFI.h"
#include "HAL_Uart.h"
#include "HAL_System.h"
#include "HAL_Flash.h"

#include "USER_Config.h"

#include "thing_init.h"
#include "thing_iot.h"
#include "thing_mqtt.h"
#include "thing_event.h"
#include "thing_property.h"

#include "lyy_device.h"
#include "lyy_adh814e.h"
#include "lyy_tjc.h"
#include "lyy_para.h"

#include "utils.h"

DEVICE_RUN_DATA_DEF   data_814_t;
DEVICE_RUN_STATE_DEF  state_814_t;

uint8_t checkSta_flag=0;          //通信正常标识
#define TIME_INTERRUPT_MS (50)    // 定时器周期
#define PERIOD_FOR_CHECKSTA (500) // 获取设备状态的周期
static int timeout_to_checkSta = PERIOD_FOR_CHECKSTA / TIME_INTERRUPT_MS;



// 查表CRC  表格
const unsigned int CrcTbl[256] =
    {
        0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
        0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
        0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
        0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
        0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
        0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
        0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
        0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
        0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
        0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
        0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
        0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
        0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
        0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
        0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
        0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
        0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
        0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
        0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
        0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
        0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
        0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
        0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
        0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
        0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
        0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
        0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
        0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
        0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
        0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
        0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
        0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040};
// 查表法计算 CRC-16
unsigned int crcVal(unsigned char *pcMess, unsigned int wLen)
{
    unsigned int Index;
    unsigned int nCRCData;
    nCRCData = 0xffff;
    while (wLen--)
    {
        Index = nCRCData >> 8;
        Index = Index ^ (*pcMess++ & 0x00ff);
        nCRCData = (nCRCData ^ CrcTbl[Index]) & 0x00ff;
        nCRCData = (nCRCData << 8) | (CrcTbl[Index] >> 8);
    }
    return (nCRCData >> 8 | nCRCData << 8);
}


void test_crc(void)
{
    uint8_t test_data[5] = {0x02,0x05,0x00,0x00,0x00};
    int CRC16DATA;

    CRC16DATA = crcVal(test_data,3);

    data_little16Toarray((uint16_t)CRC16DATA, &(test_data[3]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, test_data, 5, 500);
}

void test_crc2(void)
{
    uint8_t test_data[7] = {0x02, 0x04, 0x01, 0x00, 0x08,0x00,0x00};
    int CRC16DATA;

    CRC16DATA = crcVal(test_data, 5);

    data_little16Toarray((uint16_t)CRC16DATA, &(test_data[5]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, test_data, 7, 500);
}

void adh814e_get_id(void)   //查询板子ID编号
{
    uint8_t cmd_data[4] = {BOARD_ADDR, 0x01, 0x00, 0x00};

    data_little16Toarray((uint16_t)crcVal(cmd_data, sizeof(cmd_data)-2), &(cmd_data[2]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

void adh814e_cmd_scan(void) // 扫描门反馈指令  指令发送后，大约需要 100-130ms 应答结果，该指令执行时不能响应出货指令。
{
    uint8_t cmd_data[4] = {BOARD_ADDR, 0x02, 0x00, 0x00};

    data_little16Toarray((uint16_t)crcVal(cmd_data, sizeof(cmd_data)-2), &(cmd_data[2]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

void adh814e_cmd_poll(void)    //轮询查询状态
{
    uint8_t cmd_data[4] = {BOARD_ADDR, 0x03, 0x00, 0x00};

    data_little16Toarray((uint16_t)crcVal(cmd_data, sizeof(cmd_data)-2), &(cmd_data[2]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

void adh814e_cmd_temp(uint8_t mode, int16_t temp) // 设置温度  温度是有符号数据
{
    uint8_t cmd_data[7] = {BOARD_ADDR, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};

    cmd_data[2] = mode;

    cmd_data[3] = temp>>8;
    cmd_data[4] = temp&0x00ff;

    data_little16Toarray((uint16_t)crcVal(cmd_data, sizeof(cmd_data) - 2), &(cmd_data[5]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

void adh814e_cmd_run(uint8_t motor_num) // 启动电机运行
{
    uint8_t cmd_data[5] = {BOARD_ADDR, 0x05, 0x00, 0x00, 0x00};

    cmd_data[2] = motor_num;

    data_little16Toarray((uint16_t)crcVal(cmd_data, sizeof(cmd_data)-2), &(cmd_data[3]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

void adh814e_cmd_ack(void) // ACK 指令，在查询到出货结束后，发送该指令
{
    uint8_t cmd_data[4] = {BOARD_ADDR, 0x06, 0x00, 0x00};

    data_little16Toarray((uint16_t)crcVal(cmd_data, sizeof(cmd_data) - 2), &(cmd_data[2]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

void adh814e_cmd_run2(uint8_t motor_num1, uint8_t motor_num2) // 启动两个电机
{
    uint8_t cmd_data[6] = {BOARD_ADDR, 0x15, 0x00, 0x00, 0x00, 0x00};

    cmd_data[2] = motor_num1;
    cmd_data[3] = motor_num2;

    data_little16Toarray((uint16_t)crcVal(cmd_data, sizeof(cmd_data) - 2), &(cmd_data[4]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}


void adh814e_cmd_mode(void) // 设置814板的模式  
{
    uint8_t cmd_data[6] = {BOARD_ADDR, 0x21, 0x80, 0x00, 0x00, 0x00};

    data_little16Toarray((uint16_t)crcVal(cmd_data, sizeof(cmd_data) - 2), &(cmd_data[4]));

    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief
 *
 * @param arg
 */
void thread_adh814e_inquire_status(void *arg)
{
    (void)arg;
    while (1)
    {
        LYY_LOGD("state_814_t.state ==[%d]  shj_pay_start_t.startflag=[%d]", state_814_t.state, shj_pay_start_t.startflag);
        if (state_814_t.state == 2 && shj_pay_start_t.startflag == 0) // 出货结束，发送ACK指令
        {
            timeout_to_checkSta = PERIOD_FOR_CHECKSTA / TIME_INTERRUPT_MS; // 防止和POLL返回指令叠包
            adh814e_cmd_ack();
            LYY_LOGD("adh814e_cmd_ack1111111111111");
        }
        // adh814e_cmd_run(0);
        // adh814e_cmd_run(1);
        // adh814e_cmd_run(2);
        // adh814e_cmd_run(3);
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

/*获取设备状态倒计时清零，立即触发*/
// static void trigger_checkSta_timeout(void)
// {
//     if (flagLoopCheckSta)
//         timeout_to_checkSta = 0;
//     else
//         transmit_cmd_bitStatusGet();
// }

void device_adh814e_update_status(void *arg)
{
    (void)arg;
    if (1)      //为后续是否关闭定时检测预留
    {
        if (timeout_to_checkSta)
        {
            timeout_to_checkSta--;
        }
        if (timeout_to_checkSta <= 0)
        {
            //memset(&state_814_t, 0, sizeof(state_814_t));
            checkSta_flag = 0;
            adh814e_cmd_poll();
            LYY_LOGD("adh814e_cmd_poll2222222222222");
            timeout_to_checkSta = PERIOD_FOR_CHECKSTA / TIME_INTERRUPT_MS;
        }
    }
}
HAL_RTOS_TIMER_T g_adh814eTimerHandle;

/**
 * @brief
 *
 */
void lyy_device_adh814e_init(void)
{

    adh814e_cmd_mode();  //从新设置814板的模式

    HAL_vRtosTimerCreate(&g_adh814eTimerHandle,
                         "MS_TIMER",
                         NULL,
                         TIME_INTERRUPT_MS,
                         1,
                         device_adh814e_update_status);

    xTaskCreate(thread_adh814e_inquire_status,
                "thread_adh814e_inquire_status",
                1024 * 2, 
                NULL,
                5,
                NULL);
}

int device_adh814e_pay_apply(void)
{
    uint8_t i=10;
    while (--i)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        
        if (checkSta_flag == 1)    //防止和814断开链接，增加标志位
            break;
    }
    
    if (i <= 0)
    {
        g_xlyyHwMark.applyResult = 5;
        LYY_LOGD("g_xlyyHwMark.applyResult = 5");
        return 0;
    }
    else if (state_814_t.state == 0)
    {
        LYY_LOGD("g_xlyyHwMark.applyResult = 1");
        g_xlyyHwMark.applyResult = 1;
        return 1;
    }
    else if (state_814_t.state == 1)   //出货中
    {
        LYY_LOGD("g_xlyyHwMark.applyResult = 4");
        g_xlyyHwMark.applyResult = 4;
        return 1;
    }
    else if (state_814_t.state == 2) //出货结束，需要ACK应答清除指令
    {
        LYY_LOGD("g_xlyyHwMark.applyResult111 = 0");
        g_xlyyHwMark.applyResult = 0;
        return 1;
    }
    else
    {
        LYY_LOGD("g_xlyyHwMark.applyResult222 = 0");
        g_xlyyHwMark.applyResult = 0;
        return 1;
    }
}

int device_adh814e_pay_statr(uint16_t motor_num)
{
    uint8_t i = 10;
    while (--i)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        if (checkSta_flag == 1) // 防止和814断开链接，增加标志位 //防止和POLL返回指令叠包
            break;
    }

    if (motor_num>0)
    {
        motor_num = motor_num - 1;  //电机驱动号，为售货机实际标号-1；
        timeout_to_checkSta = PERIOD_FOR_CHECKSTA / TIME_INTERRUPT_MS; //防止和POLL返回指令叠包
        adh814e_cmd_run((uint8_t)motor_num);
    }
        


    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 3000); // 等待主板启动应答
}

/**
 * @brief
 *
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_adh814e_packet_parse(uint8_t *data, uint16_t len)
{
    uint8_t crcdata[2] = {0};
    data_little16Toarray((uint16_t)crcVal(data, len - 2), crcdata);
    LYY_LOGD("device_adh814e_packet_parse");
    if (crcdata[0] != data[len - 2] || crcdata[1] != data[len - 1])   //校验CRC
    {
        lyy_printf_hex(data, len);
        LYY_LOGD("adh814e crc fail");
        return;
    }

    if (data[0] == BOARD_ADDR && data[1] == 0x01) // 返回板卡号
    {
        memcpy(data_814_t.versions, &(data[2]), len - 4); // 
    }

    if (data[0] == 0x00)
    {
        switch (data[1])
        {
        case 0x02: // 扫描门反馈指令
            {
               memcpy(data_814_t.door_state, &(data[2]), len - 4);
            }
            break;
        case 0x03: // poll轮询数据返回
            {
                memcpy(&state_814_t, &(data[2]), sizeof(state_814_t));
                checkSta_flag = 1;  
            }
            break;
        case 0x04: // 温度控制指令返回
            /* code */
            break;
        case 0x05: // 启动电机1 应答
            {
                if (data[2]==0)
                {
                    g_xlyyHwMark.startResult = 1;
                }
                else if (data[2] == 1)
                {
                    g_xlyyHwMark.startResult = 0;
                }else{
                    g_xlyyHwMark.startResult = 4;
                }

                if (g_xlyyHwMark.ucPaySem)
                {
                    LYY_LOGD("g_xlyyHwMark.startResult[SHJ333]:%d", g_xlyyHwMark.startResult);
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0x00;
                }
            }
            break;
        case 0x06: // 出货结束ACK应答应答
            /* code */
            break;
        case 0x15:
            /* code */
            break;
        default:
            break;
        }
    }
}