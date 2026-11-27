/*
 * @author: wu gengqi
 * @Date: 2026-06-11 
 * @LastEditors: wu gengqi
 * @LastEditTime: 2026-06-11
 * @Description: 宇楠售货机设备驱动
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "cJSON.h"

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
#include "lyy_start.h"

#include "utils.h"

static uint8_t g_yunanshj_pay_index = 0;   // 支付序号
static uint8_t g_yunanshj_stock[12] = {0}; // 12个货道库存
static uint8_t g_yunanshj_wait_dispense_result = 0;//作用于支付申请流程中，标识已经下发出货指令，正在等待出货结果返回，避免在等待过程中重复下发出货指令
static volatile uint8_t g_yunanshj_detect_query_waiting = 0;
static volatile uint8_t g_yunanshj_detect_query_done = 0;
static volatile uint8_t g_yunanshj_drop_detect_enabled = 1;
static volatile uint8_t g_yunanshj_detect_property_report_pending = 0;
// 硬币和纸币合并统计，POS 保留独立的支付方式和计时窗口。
typedef struct
{
    uint64_t amount;
    uint32_t last_ms;
} yunanshj_receipts_t;

static yunanshj_receipts_t g_yunanshj_receipts[2];
#define YUNANSHJ_RECEIPT_WINDOW_MS 5000u
#define YUNANSHJ_RECEIPT_POLL_MS 100u
// 记录最近的线上出货指令，等待结束后仍能识别其重发回包。
static volatile uint32_t g_yunanshj_online_dispense_key = 0;

#define YUNANSHJ_MIN_PACKET_LEN 7
#define YUNANSHJ_RX_BUFFER_SIZE 256
#define YUNANSHJ_DETECT_QUERY_TIMEOUT_MS 1000
#define YUNANSHJ_DISPENSE_RESULT_TIMEOUT_MS 25000
#define YUNANSHJ_WAIT_STEP_MS 100
#define YUNANSHJ_CHANNEL_MAX 12
#define YUNANSHJ_STOCK_MAX 255
#define YUNANSHJ_PRICE_MAX 65535

static uint8_t g_yunanshj_rx_buffer[YUNANSHJ_RX_BUFFER_SIZE] = {0};
static uint16_t g_yunanshj_rx_len = 0;

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

static uint32_t yunanshj_dispense_key(uint8_t channel, uint8_t index)
{
    return 0x10000u | ((uint32_t)channel << 8) | index;
}

// 调用方在临界区内检查并清空；实际 MQTT 上报在临界区外执行。
static uint64_t yunanshj_take_expired_receipts(yunanshj_receipts_t *receipts, uint32_t now)
{
    if (receipts->amount == 0 || (uint32_t)(now - receipts->last_ms) < YUNANSHJ_RECEIPT_WINDOW_MS)
    {
        return 0;
    }

    uint64_t amount = receipts->amount;
    receipts->amount = 0;
    return amount;
}

static void yunanshj_report_receipts(uint64_t amount, uint8_t pos)
{
    if (amount != 0)
    {
        // 收款事件不绑定货道，也不增加出货数量。
        event_post_account_ynshj((double)amount, 0, 0, 0, pos ? "pos_pay" : "offline_pay", NULL);
    }
}

static void yunanshj_receipts_poll(void *arg)
{
    (void)arg;

    taskENTER_CRITICAL();
    uint32_t now = (uint32_t)HAL_ulGetTimeMs();
    uint64_t cash = yunanshj_take_expired_receipts(&g_yunanshj_receipts[0], now);
    uint64_t pos = yunanshj_take_expired_receipts(&g_yunanshj_receipts[1], now);
    taskEXIT_CRITICAL();

    yunanshj_report_receipts(cash, 0);
    yunanshj_report_receipts(pos, 1);
}

void device_yunanshj_init(void)
{
    static struct HalSoftTimer receipts_timer;
    HAL_vSoftTimeInit(&receipts_timer, yunanshj_receipts_poll, YUNANSHJ_RECEIPT_POLL_MS, NULL);
}

static void yunanshj_record_offline_payment(uint8_t pos, uint16_t amount)
{
    // 新线下支付是交易边界，允许主板复用线上出货的货道和序号。
    if (!g_yunanshj_wait_dispense_result)
    {
        g_yunanshj_online_dispense_key = 0;
    }

    yunanshj_receipts_t *receipts = &g_yunanshj_receipts[pos];
    taskENTER_CRITICAL();
    uint32_t now = (uint32_t)HAL_ulGetTimeMs();
    // 即使定时扫描还没执行，超过五秒的新投币也要开启新窗口。
    uint64_t expired = yunanshj_take_expired_receipts(receipts, now);
    receipts->amount += amount;
    receipts->last_ms = now;
    taskEXIT_CRITICAL();

    yunanshj_report_receipts(expired, pos);
}

static void yunanshj_report_offline_dispense(uint8_t channel)
{
    if (channel < 1 || channel > YUNANSHJ_CHANNEL_MAX)
    {
        return;
    }

    event_post_account_ynshj(0, 1, channel, 0, "offline_pay", NULL);
    if (g_yunanshj_stock[channel - 1] > 0)
    {
        g_yunanshj_stock[channel - 1]--;
    }
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
 * @brief 设置出货掉货检测查询等待标志，等待售货板返回查询结果后上报
 */
void yunanshj_dispense_detect_query_report_pending(void)
{
    g_yunanshj_detect_query_done = 0;
    g_yunanshj_detect_query_waiting = 1;
    g_yunanshj_detect_property_report_pending = 1;
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
    if (ucCh == NULL || ucSt == NULL || ucPriceH == NULL || ucPriceL == NULL || n == 0)
    {
        LYY_LOGD("set channel stock and price param error");
        return;
    }

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

static uint8_t yunanshj_number_to_u8(cJSON *item, const char *name, uint8_t max)
{
    cJSON *value = cJSON_GetObjectItem(item, name);
    if (value == NULL || value->type != cJSON_Number)
    {
        return 0;
    }

    if (value->valueint <= 0)
    {
        return 0;
    }

    if (value->valueint > max)
    {
        return max;
    }

    return (uint8_t)value->valueint;
}

/**
 * @brief 从云端下发的 records[*] 中读取 protocolPrice（整数，价格协议值），转为 16Bit 价格
 *
 * @param item records 中的单条货道 JSON 对象
 * @return uint16_t 协议价格。0 表示该字段缺失/非法（视作禁用）。
 *                 云端合法下发的最小值为 1，超过 65535 会被截断到 65535。
 */
static uint16_t yunanshj_protocol_price_to_u16(cJSON *item)
{
    cJSON *value = cJSON_GetObjectItem(item, "protocolPrice");

    if (value == NULL || value->type != cJSON_Number)
    {
        return 0;
    }

    if (value->valueint <= 0)
    {
        return 0;
    }

    if (value->valueint > YUNANSHJ_PRICE_MAX)
    {
        return YUNANSHJ_PRICE_MAX;
    }

    return (uint16_t)value->valueint;
}

int yunanshj_set_channel_stock_and_price_from_position_info(cJSON *position_info)
{
    cJSON *root = NULL;
    cJSON *records = NULL;
    uint8_t ch[YUNANSHJ_CHANNEL_MAX] = {0};
    uint8_t stock[YUNANSHJ_CHANNEL_MAX] = {0};
    uint8_t price_h[YUNANSHJ_CHANNEL_MAX] = {0};
    uint8_t price_l[YUNANSHJ_CHANNEL_MAX] = {0};
    uint8_t cnt = 0;

    if (position_info == NULL)
    {
        return -1;
    }

    if (position_info->type == cJSON_String)
    {
        root = cJSON_Parse(position_info->valuestring);
        if (root == NULL)
        {
            LYY_LOGD("yunanshj position_info parse fail");
            return -2;
        }
    }
    else
    {
        root = position_info;
    }

    if (root->type == cJSON_Array)
    {
        records = root;
    }
    else
    {
        records = cJSON_GetObjectItem(root, "records");
    }

    if (records == NULL || records->type != cJSON_Array)
    {
        if (position_info->type == cJSON_String)
        {
            cJSON_Delete(root);
        }
        LYY_LOGD("yunanshj position records error");
        return -3;
    }

    for (uint8_t i = 0; i < cJSON_GetArraySize(records) && cnt < YUNANSHJ_CHANNEL_MAX; i++)
    {
        cJSON *item = cJSON_GetArrayItem(records, i);
        uint8_t channel = 0;
        uint16_t price = 0;

        if (item == NULL || item->type != cJSON_Object)
        {
            continue;
        }

        cJSON *channel_value = cJSON_GetObjectItem(item, "positionX");
        if (channel_value == NULL || channel_value->type != cJSON_Number ||
            channel_value->valueint <= 0 || channel_value->valueint > YUNANSHJ_CHANNEL_MAX)
        {
            continue;
        }
        channel = (uint8_t)channel_value->valueint;

        stock[cnt] = yunanshj_number_to_u8(item, "stock", YUNANSHJ_STOCK_MAX);
        price = yunanshj_protocol_price_to_u16(item);
        ch[cnt] = channel;
        price_h[cnt] = (uint8_t)((price >> 8) & 0xFF);
        price_l[cnt] = (uint8_t)(price & 0xFF);
        g_yunanshj_stock[channel - 1] = stock[cnt];

        LYY_LOGD("Yunanshj lane %d stock=%d protocolPrice=%d", ch[cnt], stock[cnt], price);
        cnt++;
    }

    if (position_info->type == cJSON_String)
    {
        cJSON_Delete(root);
    }

    if (cnt == 0)
    {
        LYY_LOGD("yunanshj no valid channel data");
        return -4;
    }

    yunanshj_cmd_set_channel_stock_and_price(ch, stock, price_h, price_l, cnt);
    return cnt;
}

/**
 * @brief 支付申请
 * 流程：1.读取库存检查货道是否有货 2.下发支付金额
 * @return int 0=信号量获取失败(超时或参数错误), 1=成功(需检查applyResult)
 */
int device_yunanshj_pay_apply(uint8_t apply_channel)
{
    uint8_t channel;  // 货道号 (1-12)

    // 递增支付序号
    g_yunanshj_pay_index++;
    if (g_yunanshj_pay_index == 0) g_yunanshj_pay_index = 1;  // 防止为0

    // Use the channel parsed from cloud startup_item.positionX.
    channel = apply_channel;
    LYY_LOGD("Pay apply channel from cloud: %d", channel);
    if (channel < 1 || channel > 12) {
        LYY_LOGD("Invalid channel: %d", channel);
        g_xlyyHwMark.applyResult=PAY_APPLY_RESULT_FAULT;
        return 1;
    }

    // 第一步：检查库存
    g_xlyyHwMark.ucPaySem = 1;
    yunanshj_cmd_read_stock();  // 发送 0x26

    if (xSemaphoreTake(g_xlyyHwMark.pxPayApply, 3000) == pdFALSE) {
        g_xlyyHwMark.ucPaySem = 0;
        LYY_LOGD("Stock query timeout");
        return 0;
    }

    // 检查对应货道库存
    if (g_yunanshj_stock[channel - 1] == 0) {
        LYY_LOGD("Channel %d stock is 0", channel);
        g_xlyyHwMark.applyResult = PAY_APPLY_RESULT_FAIL;
        return 1;
    }

    // 云端支付模式下不下发 0x2C 金额，有库存即可允许支付。
    g_xlyyHwMark.applyResult = PAY_APPLY_RESULT_SUCCESS;
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
                if (data[5] == 0) {
                    LYY_LOGD("Dispense command accepted");
                } else if (data[5] == 0xED) {
                    g_xlyyHwMark.startResult = PAY_START_RESULT_FAIL;
                    LYY_LOGD("Insufficient balance");
                } else if (data[5] == 0xE0) {
                    g_xlyyHwMark.startResult = PAY_START_RESULT_FAIL;
                    LYY_LOGD("Inventory is 0");
                } else {
                    g_xlyyHwMark.startResult = PAY_START_RESULT_FAULT;
                    LYY_LOGD("Motor fault");
                }
                // ucStatus=0 只表示接收数据正常，继续等待最终出货结果。
                if(data[5] != 0 && g_yunanshj_wait_dispense_result && g_xlyyHwMark.ucPaySem) {
                    g_yunanshj_wait_dispense_result = 0;
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0;
                }
            }
            break;
        case 0x25://售货板返回FA 00 09 00 25 ucCh ucIndex ucCrc AF,售货板报告给上位机，出货物完成
            {
                if (len != 9)
                {
                    LYY_LOGD("Invalid dispense complete packet length: %d", len);
                    break;
                }
                uint8_t online_dispense = (g_yunanshj_online_dispense_key == yunanshj_dispense_key(data[5], data[6]));
                LYY_LOGD("Dispense complete, channel: %d, index: %d", data[5], data[6]);
                //发送FA 00 07 00 25 ucCrc AF给售货板,
                uint8_t cmd_data[7] = {0xFA, 0x00, 0x07, 0x00, 0x25, 0x00, 0xAF};
                cmd_data[5] = ucCrcVal(&cmd_data[1], 4); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
                if(data[5] == shj_pay_start_t.order_item[0].positionX &&
                   data[6] == g_yunanshj_pay_index &&
                   g_yunanshj_wait_dispense_result &&
                   g_xlyyHwMark.ucPaySem) {
                    if (g_yunanshj_drop_detect_enabled)
                    {
                        LYY_LOGD("Dispense complete, wait drop detect");
                    }
                    else
                    {
                        g_xlyyHwMark.startResult = PAY_START_RESULT_SUCCESS;
                        g_yunanshj_wait_dispense_result = 0;
                        xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                        g_xlyyHwMark.ucPaySem = 0;
                    }
                }
                if (!online_dispense)
                {
                    yunanshj_report_offline_dispense(data[5]);
                }
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
                if(!g_yunanshj_wait_dispense_result && g_xlyyHwMark.ucPaySem) {
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
                if(data[5] == shj_pay_start_t.order_item[0].positionX &&
                   data[6] == g_yunanshj_pay_index &&
                   g_yunanshj_wait_dispense_result &&
                   g_xlyyHwMark.ucPaySem) {
                    g_xlyyHwMark.startResult = PAY_START_RESULT_FAULT;
                    g_yunanshj_wait_dispense_result = 0;
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0;
                }
            }
            break;
        case 0x2A :// 售货板返回FA 00 09 00 2A ucCh ucIndex ucCrc AF掉货检测到有物品掉落
            {
                LYY_LOGD("Dispense detected, channel: %d, index: %d", data[5], data[6]);
                //发送FA 00 08 00 2A ucCh ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x2A, data[5], 0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
                if(data[5] == shj_pay_start_t.order_item[0].positionX &&
                   data[6] == g_yunanshj_pay_index &&
                   g_yunanshj_wait_dispense_result &&
                   g_xlyyHwMark.ucPaySem) {
                    g_xlyyHwMark.startResult = PAY_START_RESULT_SUCCESS;
                    g_yunanshj_wait_dispense_result = 0;
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0;
                }
            }
            break;
        case 0x2B: // 售货板返回FA 00 09 00 2B ucCh ucIndex ucCrc AF弹簧转一圈，掉货检测没有物品掉落，等待超时
            {
                LYY_LOGD("Dispense failure, channel: %d, index: %d", data[5], data[6]);
                //发送FA 00 08 00 2B ucCh ucCrc AF给售货板
                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, 0x2B, data[5], 0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5); // 校验位，从从机地址到数据信息进行异或得到的值
                HAL_vUartSend(HAL_UART_MAIN_BOARD, cmd_data, sizeof(cmd_data), 500);
                if(data[5] == shj_pay_start_t.order_item[0].positionX &&
                   data[6] == g_yunanshj_pay_index &&
                   g_yunanshj_wait_dispense_result &&
                   g_xlyyHwMark.ucPaySem) {
                    g_xlyyHwMark.startResult = PAY_START_RESULT_FAIL;
                    g_yunanshj_wait_dispense_result = 0;
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0;
                }
            }
            break;
        case 0x2c: // 售货板返回FA 00 08 00 2c ucStatus ucCrc AF 参数：ucStatus：成功为 1，失败为 2
            {
                g_xlyyHwMark.applyResult = data[5];  // 1=成功, 2=失败
                if(data[5]==1)LYY_LOGD("Set payment amount success");
                else LYY_LOGD("Set payment amount failure");
                // 释放信号量
                if(!g_yunanshj_wait_dispense_result && g_xlyyHwMark.ucPaySem) {
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
                if (g_yunanshj_detect_query_waiting)
                {
                    g_yunanshj_drop_detect_enabled = (data[5] == 1) ? 1 : 0;
                    g_yunanshj_detect_query_done = 1;
                    g_yunanshj_detect_query_waiting = 0;
                    if(g_yunanshj_drop_detect_enabled)LYY_LOGD("Dispense detect enabled");
                    else LYY_LOGD("Dispense detect disabled");
                    if(g_yunanshj_detect_property_report_pending)
                    {
                        g_yunanshj_detect_property_report_pending = 0;
                        property_post_vending_drop_detection(g_yunanshj_drop_detect_enabled);
                    }
                }
            }
            break;
        case 0x32: // 售货板返回FA 00 07 00 32 ucCrc AF
            {
                LYY_LOGD("Set dispense detect reply");
                if(g_yunanshj_detect_property_report_pending)
                {
                    yunanshj_cmd_query_dispense_detect_status();
                }
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
        case 0x36: // 纸币
        case 0x37: // POS 刷卡
        case 0x38: // 硬币
            {
                if (len != 10)
                {
                    LYY_LOGD("Invalid offline payment packet length: %d", len);
                    break;
                }
                uint16_t money = data[5] * 256 + data[6];
                LYY_LOGD("Offline payment cmd=%02X, amount=%d, index=%d", cmd, money, data[7]);
                yunanshj_record_offline_payment(cmd == 0x37, money);

                uint8_t cmd_data[8] = {0xFA, 0x00, 0x08, 0x00, cmd, 0x01, 0x00, 0xAF};
                cmd_data[6] = ucCrcVal(&cmd_data[1], 5);
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
void device_yunanshj_stream_parse(uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0)
    {
        return;
    }

    if (len > YUNANSHJ_RX_BUFFER_SIZE)
    {
        LYY_LOGD("Yunanshj uart data too long: %d", len);
        g_yunanshj_rx_len = 0;
        return;
    }

    if ((g_yunanshj_rx_len + len) > YUNANSHJ_RX_BUFFER_SIZE)
    {
        LYY_LOGD("Yunanshj rx buffer overflow, drop cached data");
        g_yunanshj_rx_len = 0;
    }

    memcpy(&g_yunanshj_rx_buffer[g_yunanshj_rx_len], data, len);
    g_yunanshj_rx_len += len;

    while (g_yunanshj_rx_len > 0)
    {
        uint16_t start = 0;
        while (start < g_yunanshj_rx_len && g_yunanshj_rx_buffer[start] != 0xFA)
        {
            start++;
        }

        if (start >= g_yunanshj_rx_len)
        {
            g_yunanshj_rx_len = 0;
            return;
        }

        if (start > 0)
        {
            memmove(g_yunanshj_rx_buffer, &g_yunanshj_rx_buffer[start], g_yunanshj_rx_len - start);
            g_yunanshj_rx_len -= start;
        }

        if (g_yunanshj_rx_len < 3)
        {
            return;
        }

        uint8_t packet_len = g_yunanshj_rx_buffer[2];
        if (packet_len < YUNANSHJ_MIN_PACKET_LEN)
        {
            LYY_LOGD("Invalid yunanshj packet length: %d", packet_len);
            memmove(g_yunanshj_rx_buffer, &g_yunanshj_rx_buffer[1], g_yunanshj_rx_len - 1);
            g_yunanshj_rx_len--;
            continue;
        }

        if (g_yunanshj_rx_len < packet_len)
        {
            return;
        }

        if (g_yunanshj_rx_buffer[packet_len - 1] != 0xAF)
        {
            LYY_LOGD("Invalid yunanshj packet end");
            memmove(g_yunanshj_rx_buffer, &g_yunanshj_rx_buffer[1], g_yunanshj_rx_len - 1);
            g_yunanshj_rx_len--;
            continue;
        }

        device_yunanshj_packet_parse(g_yunanshj_rx_buffer, packet_len);

        if (g_yunanshj_rx_len == packet_len)
        {
            g_yunanshj_rx_len = 0;
            return;
        }

        memmove(g_yunanshj_rx_buffer, &g_yunanshj_rx_buffer[packet_len], g_yunanshj_rx_len - packet_len);
        g_yunanshj_rx_len -= packet_len;
    }
}

/**
 * @brief Send dispense command for cloud payment start.
 *
 * @return int 0=timeout, 1=success (check startResult)
 */
int device_yunanshj_pay_start(void)
{
    uint8_t channel = shj_pay_start_t.order_item[0].positionX;
    uint32_t waited_ms = 0;
    uint32_t query_waited_ms = 0;

    LYY_LOGD("Dispense command: channel=%d, index=%d", channel, g_yunanshj_pay_index);

    g_xlyyHwMark.ucPaySem = 0;
    g_yunanshj_wait_dispense_result = 0;
    g_yunanshj_detect_query_done = 0;
    g_yunanshj_detect_query_waiting = 1;
    g_yunanshj_detect_property_report_pending = 0;
    g_yunanshj_drop_detect_enabled = 1;
    yunanshj_cmd_query_dispense_detect_status();
    while (!g_yunanshj_detect_query_done && query_waited_ms < YUNANSHJ_DETECT_QUERY_TIMEOUT_MS)
    {
        vTaskDelay(YUNANSHJ_WAIT_STEP_MS / portTICK_PERIOD_MS);
        query_waited_ms += YUNANSHJ_WAIT_STEP_MS;
    }

    if (!g_yunanshj_detect_query_done)
    {
        g_yunanshj_detect_query_waiting = 0;
        g_yunanshj_drop_detect_enabled = 0;
        LYY_LOGD("Dispense detect query timeout, use disabled mode");
    }

    while (xSemaphoreTake(g_xlyyHwMark.pxPayApply, 0) != pdFALSE)
    {
    }

    g_xlyyHwMark.ucPaySem = 1;
    g_yunanshj_wait_dispense_result = 1;
    g_yunanshj_online_dispense_key = yunanshj_dispense_key(channel, g_yunanshj_pay_index);
    yunanshj_cmd_dispense(channel, g_yunanshj_pay_index);

    while (waited_ms < YUNANSHJ_DISPENSE_RESULT_TIMEOUT_MS)
    {
        if (xSemaphoreTake(g_xlyyHwMark.pxPayApply, YUNANSHJ_WAIT_STEP_MS) != pdFALSE)
        {
            return 1;
        }

        waited_ms += YUNANSHJ_WAIT_STEP_MS;
    }

    g_xlyyHwMark.ucPaySem = 0;
    g_yunanshj_wait_dispense_result = 0;
    LYY_LOGD("Dispense result timeout");
    return 0;
}
