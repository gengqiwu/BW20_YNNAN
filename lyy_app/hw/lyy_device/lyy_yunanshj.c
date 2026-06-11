/*
 * @author: wu gengqi
 * @Date: 2026-06-11 
 * @LastEditors: wu gengqi
 * @LastEditTime: 2026-06-11
 * @Description: 宇楠售货机设备驱动
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
#include "lyy_yunanshj.h"
#include "lyy_tjc.h"
#include "lyy_para.h"

#include "utils.h"

static uint8_t g_yunanshj_pay_index = 0;   // 支付序号
static uint8_t g_yunanshj_stock[12] = {0}; // 12个货道库存

/** 宇楠售货机传输数据协议：
格式：
主机 文件头（FA） 从机地址（00） 数据长度 0 命令 数据信息 校验 文件尾(AF)
从机 文件头(FA) 主机地址 数据长度 从机地址(00) 命令 数据信息 校验 文件尾(AF)
**/


//计算从地址到数据信息的所有数据进行异或后得到的值
int ucCrcVal(uint8_t *data, uint16_t len)
{
    int crc = 0;
    for (int i = 0; i < len; i++)
    {
        crc ^= data[i];
    }
    return crc;
}

/**
 * @brief 命令售货板从货道 ucNum 中输出物品 命令 0x24：FA 00 09 00 24 ucCh ucIndex ucCrc AF
 *
 * @param ucCh 货道号>0
 * @param ucIndex：> 0 区分是否已经下发过出货指令，每次出货后增加 1（或者随机数）
 */
void yunanshj_cmd_dispense(uint8_t ucCh, uint8_t ucIndex)
{
    uint8_t cmd_data[9] = {0xFA, 0x00, 0x09, 0x00, 0x24, ucCh, ucIndex, 0x00, 0xAF};
    cmd_data[7] = ucCrcVal(&cmd_data[1], 6); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 售读取库存 命令 令 0x26：FA 00 07 00 26 ucCrc AF
 *
 * @param 无
 */
void yunanshj_cmd_read_stock(void)
{
    uint8_t cmd_data[7] = {0xFA, 0x00, 0x07, 0x00, 0x26, 0x00, 0xAF};
    cmd_data[5] = ucCrcVal(&cmd_data[1], 4); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 下发支付金额 命令 0x2C：FA 00 0a 00 2C ucMoneyH ucMoneyL ucIndex ucCrc AF
 *
 * @param ucMoneyH ucMoneyL：金额，金额=ucMoneyH * 256 + ucMoneyL
 * @param ucIndex: > 0，区分是否已经下发过出货指令，每次出货后增加 1（或者随机数）
 */
void yunanshj_cmd_set_payment_amount(uint8_t ucMoneyH, uint8_t ucMoneyL, uint8_t ucIndex)
{
    uint8_t cmd_data[10] = {0xFA, 0x00, 0x0A, 0x00, 0x2C, ucMoneyH, ucMoneyL, ucIndex, 0x00, 0xAF};
    cmd_data[8] = ucCrcVal(&cmd_data[1], 7); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 读取支付总金额 命令 0x2D：FA 00 07 00 2D ucCrc AF
 *
 * @param 无
 */
void yunanshj_cmd_read_total_payment_amount(void)
{
    uint8_t cmd_data[7] = {0xFA, 0x00, 0x07, 0x00, 0x2D, 0x00, 0xAF};
    cmd_data[5] = ucCrcVal(&cmd_data[1], 4); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 远程补货 命令 0x2E：FA 00 [7 + 2*n] 00 2E ucCh1 ucSt1 ... ucChn ucStn ucCrc AF
 *
 * @param ucCh1 ... ucChn：货道号（> 0）
 * @param ucSt1 ... ucStn：货道补货商品数量
 */
void yunanshj_cmd_remote_restock(uint8_t *ucCh, uint8_t *ucSt, uint8_t n)
{
    uint8_t cmd_data[7 + 2 * n];
    cmd_data[0] = 0xFA;
    cmd_data[1] = 0x00;
    cmd_data[2] = 7 + 2 * n;
    cmd_data[3] = 0x00;
    cmd_data[4] = 0x2E;
    for (int i = 0; i < n; i++)
    {
        cmd_data[5 + 2 * i] = ucCh[i];
        cmd_data[6 + 2 * i] = ucSt[i];
    }
    cmd_data[5 + 2 * n] = ucCrcVal(&cmd_data[1], 4 + 2 * n); // 校验位，从从机地址到数据信息进行异或得到的值
    cmd_data[6 + 2 * n] = 0xAF;
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 查询控制板是否有应答 命令 0x30：FA 00 07 00 30 ucCrc AF
 *
 * @param 无
 */
void yunanshj_cmd_query_control_board_response(void)
{
    uint8_t cmd_data[7] = {0xFA, 0x00, 0x07, 0x00, 0x30, 0x00, 0xAF};
    cmd_data[5] = ucCrcVal(&cmd_data[1], 4); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 查询出货掉货检测是否启用 命令 0x31：FA 00 07 00 31 ucCrc AF
 *
 * @param 无
 */
void yunanshj_cmd_query_dispense_detect_status(void)
{
    uint8_t cmd_data[7] = {0xFA, 0x00, 0x07, 0x00, 0x31, 0x00, 0xAF};
    cmd_data[5] = ucCrcVal(&cmd_data[1], 4); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 设置出货掉货检测 命令 0x32：FA 00 08 00 32 data ucCrc AF
 *
 * @param data：启用掉货检测为 1，不启用掉货检测为 0
 */
void yunanshj_cmd_set_dispense_detect(uint8_t data)
{
    uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x32, data, 0x00, 0xAF};
    cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 清除货道故障状态 命令 0x33：FA 00 08 00 33 ucStatus ucCrc AF
 *
 * @param ucStatus：需要清除为 1，不需要掉货检测为 0，返回故障状态
 */
void yunanshj_cmd_clear_channel_fault_status(uint8_t ucStatus)
{
    uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x33, ucStatus, 0x00, 0xAF};
    cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 设置货道价格 命令 0x34：FA 00 [7 + 3*n] 00 34 ucCh1 ucPriceH1 ucPriceL1 ……ucChn ucPriceHn ucPriceLn ucCrc AF
 *
 * @param ucCh1 ... ucChn：货道号（> 0）
 * @param ucPriceH1 ucPriceL1 ... ucPriceHn ucPriceLn：价格（16Bit），uiPrice = ucPriceH * 256 +ucPriceLL
 */
void yunanshj_cmd_set_channel_price(uint8_t *ucCh, uint8_t *ucPriceH, uint8_t *ucPriceL, uint8_t n)
{
    uint8_t cmd_data[7 + 3 * n];
    cmd_data[0] = 0xFA;
    cmd_data[1] = 0x00;
    cmd_data[2] = 7 + 3 * n;
    cmd_data[3] = 0x00;
    cmd_data[4] = 0x34;
    for (int i = 0; i < n; i++)
    {
        cmd_data[5 + 3 * i] = ucCh[i];
        cmd_data[6 + 3 * i] = ucPriceH[i];
        cmd_data[7 + 3 * i] = ucPriceL[i];
    }
    cmd_data[5 + 3 * n] = ucCrcVal(&cmd_data[1], 4 + 3 * n); // 校验位，从从机地址到数据信息进行异或得到的值
    cmd_data[6 + 3 * n] = 0xAF;
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 金额清零 命令 0x35：FA 00 08 00 35 ucStatus ucCrc AF
 *
 * @param ucStatus：1 为金额归零，非 1 为不处理
 */
void yunanshj_cmd_clear_payment_amount(uint8_t ucStatus)
{
    uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x35, ucStatus, 0x00, 0xAF};   
    cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 读取当前金额 命令 0x39：FA 00 07 00 39 ucCrc AF
 *
 * @param 无
 */
void yunanshj_cmd_read_current_amount(void)
{
    uint8_t cmd_data[7] = {0xFA, 0x00, 0x07, 0x00, 0x39, 0x00, 0xAF};
    cmd_data[5] = ucCrcVal(&cmd_data[1], 4); // 校验位，从从机地址到数据信息进行异或得到的值
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 设置货道库存和价格 命 令 0x3A ： FA 00 [7 + 4*n] 00 3A ucCh1 ucSt1 ucPriceH1 ucPriceL1 …… ucChn ucStn ucPriceHn ucPriceLn ucCrc AF
 *
 * @param ucCh1 ... ucChn：货道号（> 0）
 * @param ucSt1...ucStn：货道库存
 * @param ucPriceH1 ucPriceL1 ... ucPriceHn ucPriceLn：价格（16Bit），uiPrice = ucPriceH * 256 +ucPriceL
 */
void yunanshj_cmd_set_channel_stock_and_price(uint8_t *ucCh, uint8_t *ucSt, uint8_t *ucPriceH, uint8_t *ucPriceL, uint8_t n)
{
    uint8_t cmd_data[7 + 4 * n];
    cmd_data[0] = 0xFA;
    cmd_data[1] = 0x00;
    cmd_data[2] = 7 + 4 * n;
    cmd_data[3] = 0x00;
    cmd_data[4] = 0x3A;
    for (int i = 0; i < n; i++)
    {
        cmd_data[5 + 4 * i] = ucCh[i];
        cmd_data[6 + 4 * i] = ucSt[i];
        cmd_data[7 + 4 * i] = ucPriceH[i];
        cmd_data[8 + 4 * i] = ucPriceL[i];
    }
    cmd_data[5 + 4 * n] = ucCrcVal(&cmd_data[1], 4 + 4 * n); // 校验位，从从机地址到数据信息进行异或得到的值
    cmd_data[6 + 4 * n] = 0xAF;
    HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
}

/**
 * @brief 支付申请
 * 流程：1.读取库存检查货道是否有货 2.下发支付金额
 * @return int 0=信号量获取失败(超时或参数错误), 1=成功(需检查applyResult)
 */
int device_yunanshj_pay_apply(void)
{
    uint8_t channel;  // 货道号 (1-12)
    uint16_t amount;   // 支付金额

    // 递增支付序号
    g_yunanshj_pay_index++;
    if (g_yunanshj_pay_index == 0) g_yunanshj_pay_index = 1;  // 防止为0

    // 获取货道号
    channel = shj_pay_start_t.order_item[0].positionX;
    if (channel < 1 || channel > 12) {
        LYY_LOGD("Invalid channel: %d", channel);
        g_xlyyHwMark.applyResult = 6;  // 货道号无效
        return 0;
    }

    // 第一步：检查库存
    g_xlyyHwMark.ucPaySem = 1;
    yunanshj_cmd_read_stock();  // 发送 0x26

    if (xSemaphoreTake(g_xlyyHwMark.pxPayApply, 3000) == pdFALSE) {
        g_xlyyHwMark.ucPaySem = 0;
        LYY_LOGD("Stock query timeout");
        g_xlyyHwMark.applyResult = 5;  // 超时
        return 0;
    }

    // 检查对应货道库存
    if (g_yunanshj_stock[channel - 1] == 0) {
        LYY_LOGD("Channel %d stock is 0", channel);
        g_xlyyHwMark.applyResult = 7;  // 库存不足
        return 1;
    }

    // 第二步：下发金额
    PAY_PACKAGES_DEF *pkgs = get_pay_packages();
    if (channel <= pkgs->sum) {
        amount = (uint16_t)pkgs->pkg[channel - 1].priceValue;
    } else {
        amount = 0;
    }

    uint8_t amountH = amount >> 8;
    uint8_t amountL = amount & 0xFF;
    LYY_LOGD("Set payment amount: channel=%d, amount=%d(0x%02X%02X)", channel, amount, amountH, amountL);

    g_xlyyHwMark.ucPaySem = 1;
    yunanshj_cmd_set_payment_amount(amountH, amountL, g_yunanshj_pay_index);

    if (xSemaphoreTake(g_xlyyHwMark.pxPayApply, 5000) == pdFALSE) {
        g_xlyyHwMark.ucPaySem = 0;
        LYY_LOGD("Set payment amount timeout");
        g_xlyyHwMark.applyResult = 5;  // 超时
        return 0;
    }

    return 1;
}



/**
 * @brief 解析售货板发送的数据
 *
 * @param data数据
 * @param len 数据长度
 */
void device_yunanshj_packet_parse(uint8_t *data, uint16_t len)
{
    LYY_LOGD("device_yunanshj_packet_parse");
    // 先进行校验，校验通过后根据命令进行处理
    if (len < 7 || data[0] != 0xFA || data[len - 1] != 0xAF)
    {
        LYY_LOGD("Invalid packet");
        return;
    }
    uint8_t crc = ucCrcVal(&data[1], len - 3); // 从从机地址到数据信息进行异或得到的值
    if (crc != data[len - 2])
    {
        LYY_LOGD("CRC check failed");
        return;
    }
    uint8_t cmd = data[4];
    switch (cmd)
    {
        case 0x24: // 售货板返回FA 00 08 00 24 ucStatus ucCrc AF
            {
                g_xlyyHwMark.startResult = data[5];  // 0=成功, 0xED=余额不足, 0xE0=库存为0, 其他=故障
                if(data[5]==0)LYY_LOGD("Dispense success");
                else if(data[5]==0xED)LYY_LOGD("Insufficient balance");
                else if(data[5]==0xE0)LYY_LOGD("Inventory is 0");
                else LYY_LOGD("Motor fault");
                // 释放信号量
                if(g_xlyyHwMark.ucPaySem) {
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0;
                }
            }
            break;
        case 0x25://售货板返回FA 00 09 00 25 ucCh ucIndex ucCrc AF,售货板报告给上位机，出货物完成
            {
                LYY_LOGD("Dispense complete, channel: %d, index: %d", data[5], data[6]);
                //发送FA 00 07 00 25 ucCrc AF给售货板,
                uint8_t cmd_data[7] = {0xFA, 0x00, 0x07, 0x00, 0x25, 0x00, 0xAF};
                cmd_data[5] = ucCrcVal(&cmd_data[1], 4); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
            }
            break;
        case 0x26: // 售货板返回FA 00 13 00 26 data1 data2 …… data12 ucCrc AF,售货板报告给上位机库存数据
            {
                memcpy(g_yunanshj_stock, &data[5], 12);  // 保存12个货道库存
                LYY_LOGD("Stock data:");
                for(int i=0;i<12;i++)
                {
                    LYY_LOGD("Channel %d: %d", i+1, g_yunanshj_stock[i]);
                }
                // 释放库存查询信号量
                if(g_xlyyHwMark.ucPaySem) {
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0;
                }
            }
            break;
        case 0x29: // 售货板返回FA 00 09 00 29 ucCh ucIndex ucCrc AF,马达或者线路故障导致没有收到弹簧转一圈的信号
            {
                LYY_LOGD("Dispense failure, channel: %d, index: %d", data[5], data[6]);
                //发送FA 00 08 00 29 ucCh ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x29, data[5], 0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
            }
            break;
        case 0x2A :// 售货板返回FA 00 08 00 2A ucCh ucIndex ucCrc AF掉货检测到有物品掉落
            {
                LYY_LOGD("Dispense failure, channel: %d, index: %d", data[5], data[6]);
                //发送FA 00 08 00 2A ucCh ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x2A, data[5], 0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
            }
            break;
        case 0x2B: // 售货板返回FA 00 08 00 2B ucCh ucIndex ucCrc AF弹簧转一圈，掉货检测没有物品掉落，等待超时
            {
                LYY_LOGD("Dispense failure, channel: %d, index: %d", data[5], data[6]);
                //发送FA 00 08 00 2B ucCh ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x2B, data[5], 0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
            }
            break;
        case 0x2c: // 售货板返回FA 00 08 00 2c ucStatus ucCrc AF 参数：ucStatus：成功为 1，失败为 2
            {
                g_xlyyHwMark.applyResult = data[5];  // 1=成功, 2=失败
                if(data[5]==1)LYY_LOGD("Set payment amount success");
                else LYY_LOGD("Set payment amount failure");
                // 释放信号量
                if(g_xlyyHwMark.ucPaySem) {
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0;
                }
            }
            break;
        case 0x2D: // 售货板返回FA 00 09 00 2d ucMoneyH ucMoneyL ucCrc AF 参数：ucMoneyH ucMoneyL：金额，金额=ucMoneyH * 256 + ucMoneyL
            {
                uint16_t total_amount = data[5] * 256 + data[6];
                LYY_LOGD("Total payment amount: %d", total_amount);
            }
            break;
        case 0x2E: // 售货板返回FA 00 08 00 2E ucStatus ucCrc AF参数：ucStatus：成功为 1，失败为 2
            {
                if(data[5]==1)LYY_LOGD("Remote restock success");
                else LYY_LOGD("Remote restock failure");
            }
            break;
        case 0x30: // 售货板返回FA 00 07 00 30 ucCrc AF
            {
                LYY_LOGD("Control board response");
            }
            break;
        case 0x31: // 售货板返回FA 00 08 00 31 data ucCrc AF参数：data：启用掉货检测为 1，不启用掉货检测为 0
            {
                if(data[5]==1)LYY_LOGD("Dispense detect enabled");
                else LYY_LOGD("Dispense detect disabled");
            }
            break;
        case 0x32: // 售货板返回FA 00 07 00 32 ucCrc AF
            {
                LYY_LOGD("Clear channel fault status");
            }
            break;
        case 0x33: // 售货板返回FA 00 09 00 32 data1 data2 ucCrc AF参数：data1 data2：按位来表示各个货道的故障状态。Data1.3-data.0 对应货道 12-9，data2.7-data2.0 对应货道 8-1 ，对应的值为 1 则货道状态为故障，0 则正常。比如 data1=0x06（二进制 00000110），则货道 12 和 9 正常，货道 11 和 10 故障；data2=0x43（二进制值 0100 0011），则货道 8 、6 、5、 4 和 3 正常，货道 7 、 2和 1 有故障。
            {
                LYY_LOGD("Clear channel fault status");
                for(int i=0;i<12;i++)
                {
                    int byte_index = (i < 4) ? 1 : 2; // data1对应货道12-9，data2对应货道8-1
                    int bit_index = (i < 4) ? (3 - i) : (7 - (i - 4)); // 按位来表示各个货道的故障状态
                    if((data[byte_index] >> bit_index) & 0x01)
                    {
                        LYY_LOGD("Channel %d: Fault", 12 - i);
                    }
                    else
                    {
                        LYY_LOGD("Channel %d: Normal", 12 - i);
                    }
                }
            }
            break;
        case 0x34: // 售货板返回FA 00 08 00 34 ucStatus ucCrc AF
            {
                LYY_LOGD("Set channel price");
            }
            break;
        case 0x35: // 售货板返回FA 00 08 00 35 ucStatus ucMoneyH ucMoneyL ucCrc AF参数：ucMoneyH、ucMoneyL：当前金额。如果清零则返回 0，不处理则返回当前金额。
            {
                uint16_t current_amount = data[6] * 256 + data[7];
                LYY_LOGD("Clear payment amount, current amount: %d", current_amount);
            }
            break;
        case 0x36: // 售货板返回FA 00 0A 00 36 ucMoneyH ucMoneyL ucIndex ucCrc AF投现金（纸币）,参数： ucMoneyH ucMoneyL：投钱金额（16Bit），uiMoney = ucMoneyH*256+ucMoneyLucIndex：> 0,区分是否已经下发过出货指令，每次出货后增加 1（或者随机数）
            {
                uint16_t money = data[5] * 256 + data[6];
                uint8_t index = data[7];
                LYY_LOGD("Cash inserted, amount: %d, index: %d", money, index);
                //发送FA 00 08 00 36 ucStatus ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x36, 0x01,0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
            }
            break;
        case 0x37: // 售货板返回FA 00 0A 00 37 ucMoneyH ucMoneyL ucIndex ucCrc AF投刷卡刷卡金额,参数： ucMoneyH ucMoneyL：投钱金额（16Bit），uiMoney = ucMoneyH*256+ucMoneyLucIndex：>0,区分是否已经下发过出货指令，每次出货后增加 1（或者随机数）
            {
                uint16_t money = data[5] * 256 + data[6];
                uint8_t index = data[7];
                LYY_LOGD("Card payment, amount: %d, index: %d", money, index);
                //发送FA 00 08 00 37 ucStatus ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x37, 0x01,0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
            }
            break;
        case 0x38: // 售货板返回FA 00 0A 00 38 ucMoneyH ucMoneyL ucIndex ucCrc AF投现金（硬币）,参数： ucMoneyH ucMoneyL：投钱金额（16Bit），uiMoney = ucMoneyH*256+ucMoneyLucIndex：>0,区分是否已经下发过出货指令，每次出货后增加 1（或者随机数）
            {
                uint16_t money = data[5] * 256 + data[6];
                uint8_t index = data[7];
                LYY_LOGD("Coin inserted, amount: %d, index: %d", money, index);
                //发送FA 00 08 00 38 ucStatus ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x38, 0x01,0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
            }
            break;
        case 0x39: // 售货板返回FA 00 09 00 39 ucMoneyH ucMoneyL ucCrc AF参数： ucMoneyH ucMoneyL：投钱金额（16Bit），uiMoney = ucMoneyH*256+ucMoneyL
            {
                uint16_t current_amount = data[5] * 256 + data[6];
                LYY_LOGD("Current amount: %d", current_amount);
            }
            break;
        case 0x3A: // 售货板返回FA 00 08 00 3A ucStatus ucCrc AF
            {
                LYY_LOGD("Set channel stock and price");
            }
            break;
        case 0x3B: // 售货板返回FA 00 0B 00 3B ucCh ucMoneyH ucMoneyL ucIndex ucCrc AF上传按键,参数： ucCh：按键货道号（> 0）ucMoneyH ucMoneyL：投钱金额（16Bit），uiMoney = ucMoneyH*256+ucMoneyLucIndex：>0,区分是否已经下发过出货指令，每次出货后增加 1（或者随机数）
            {
                uint8_t channel = data[5];
                uint16_t money = data[6] * 256 + data[7];
                uint8_t index = data[8];
                LYY_LOGD("Button pressed, channel: %d, amount: %d, index: %d", channel, money, index);
                //发送FA 00 08 00 3B ucStatus ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x3B, 0x01,0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
            }
            break;
        case 0x3C: // 售货板返回FA 00 08 00 3C ucStatus ucCrc AF汇报异常或者状态,参数： ucStatus：设备出货中 : 1通讯异常 结尾不是 0xAF : 2通讯异常 校验错误 : 3重复发数据（标识一样） : 4
            {
                if(data[5]==1)LYY_LOGD("Device dispensing");
                else if(data[5]==2)LYY_LOGD("Communication error: Invalid packet");
                else if(data[5]==3)LYY_LOGD("Communication error: CRC check failed");
                else if(data[5]==4)LYY_LOGD("Communication error: Duplicate data");
                else LYY_LOGD("Unknown status: %d", data[5]);
                //发送FA 00 08 00 3C ucStatus ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x3C, data[5], 0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
            }
            break;
        default:
            LYY_LOGD("Unknown command: %d", cmd);
             break;
    }


}

/**
 * @brief 支付启动 - 发送出货命令
 *
 * @return int 0=信号量获取失败(超时), 1=成功(需检查startResult)
 */
int device_yunanshj_pay_statr(void)
{
    uint8_t channel = shj_pay_start_t.order_item[0].positionX;

    LYY_LOGD("Dispense command: channel=%d, index=%d", channel, g_yunanshj_pay_index);

    g_xlyyHwMark.ucPaySem = 1;
    yunanshj_cmd_dispense(channel, g_yunanshj_pay_index);

    if (xSemaphoreTake(g_xlyyHwMark.pxPayApply, 5000) == pdFALSE) {
        g_xlyyHwMark.ucPaySem = 0;
        LYY_LOGD("Dispense command timeout");
        g_xlyyHwMark.startResult = 5;  // 超时
        return 0;
    }

    return 1;
}