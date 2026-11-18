/**
 * @file APP_Factory.c
 * @brief 与 STM32 透传烧号工具兼容的 ASCII 工厂测试与设备号保存流程。
 * @note 工厂模式占用主板接收回调，不同时解析正常 D6 业务帧。
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "HAL_Log.h"
#include "HAL_Uart.h"
#include "HAL_System.h"
#include "APP_Led.h"
#include "APP_Param.h"
#include "APP_Factory.h"
#include "lyy_start.h"

#define FACTORY_PROMPT "write device_id:include(letter,number, @ - : . _)(4-32byte)"
#define FACTORY_COMMAND "write device_id:"
static uint8_t loopback;
static char command[128];
static size_t used;
static uint32_t recv_tick;
static SemaphoreHandle_t command_lock;

/**
 * @brief 保存工厂默认参数，再尝试写入设备号。
 * @param[in] id 以 NUL 结尾的设备号；NULL 时不保存，字符合法性由调用者先检查。
 * @note 默认 38400 是机台参数；默认脉宽/间隔为 50/200 ms，不改变 STM32 通信速率。
 *       本接口无写入状态返回，烧号提交流程通过重新读取参数及设备号进行检查。
 */
void APP_vParamInfoInit(char *id)
{
    APP_PARAM_T param = {0};
    param.ulBaudRate = 38400;
    strcpy(param.xWifiInfo.pcSsid, LYY_WIFI_SSID);
    strcpy(param.xWifiInfo.pcPwd, LYY_WIFI_PWD);
    param.xWifiInfo.xMode = WIFI_STA;
    param.xDevPulse.ulPulsetype = START_TYPE_COIN;
    param.xDevPulse.ulPulseWidth = 50;
    param.xDevPulse.ulPulseSpace = 200;
    if (id && APP_lParamSetInfo(&param) == LYY_SUCCESS)
        APP_vDevInfoInit(id);
}

/**
 * @brief 检查工厂设备号长度及允许的字符集合。
 * @param[in] id 待检查字节序列，至少包含 len 个可读字节。
 * @param[in] len 设备号字节数，不含终止符，允许 4～32 字节。
 * @retval 1 仅包含字母、数字及 @ - : . _。
 * @retval 0 长度或字符不符合烧号协议。
 */
static int valid_id(const char *id, size_t len)
{
    if (len < 4 || len > 32) return 0;
    for (size_t i = 0; i < len; ++i) {
        char c = id[i];
        if ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') || c == '@' || c == '-' ||
            c == ':' || c == '.' || c == '_') continue;
        return 0;
    }
    return 1;
}

/**
 * @brief 校验完整烧号命令、保存并读回设备号，成功后回复工具并复位。
 * @pre 调用者持有 command_lock，command 已补 NUL 终止符。
 * @note 成功文本必须兼容 STM32 的识别逻辑；校验失败或读回不匹配时不回复成功。
 */
static void factory_commit(void)
{
    const char *prefix = FACTORY_COMMAND;
    if (used < strlen(prefix) || strncmp(command, prefix, strlen(prefix))) return;
    char *id = command + strlen(prefix);
    if (!valid_id(id, strlen(id))) return;
    APP_vParamInfoInit(id);
    /* Acknowledge only after both parameters and device ID reached flash. */
    if (strcmp(APP_pcGetParamDevId(), id) || APP_lParamInit() != LYY_SUCCESS ||
        strcmp(APP_pcGetParamDevId(), id)) {
        LYY_LOGE("factory parameter write failed");
        return;
    }
    char response[100];
    snprintf(response, sizeof(response), "\r\ndevice id is ok,devid:%s\r\n", id);
    HAL_vUartSend(HAL_UART_MAIN_BOARD, (uint8_t *)response, strlen(response), 1000);
    HAL_vReset("> device ID_Config_OK <", 200);
}

/**
 * @brief 串口任务中处理回环提示、回显和跨接收块的 ASCII 命令累积。
 * @param[in] data 当前接收块，按 len 读取，不要求带 NUL。
 * @param[in] len 当前块字节数。
 * @note 与提交轮询共用互斥锁；超过 200 ms 的接收间隔会丢弃旧命令。
 *       CR、LF 和 NUL 不计入命令内容，提交由静默时间触发。
 */
static void factory_recv(uint8_t *data, uint32_t len)
{
    if (xSemaphoreTake(command_lock, portMAX_DELAY) != pdTRUE) return;
    if (HAL_ucTickTimeout(recv_tick, 200)) used = 0;
    recv_tick = HAL_ulGetSysTick();
    if (len && len < sizeof(command)) {
        char text[128];
        memcpy(text, data, len);
        text[len] = 0;
        if (strstr(text, "write device_id:include")) {
            loopback = 1;
            set_led_status(LED_STATUS_NO_DEVID);
            APP_vLedSstSta(LED_GREEN, LED_STA_QUICK_FLASH);
            APP_vLedSstSta(LED_RED, LED_STA_QUICK_FLASH);
            used = 0;
            xSemaphoreGive(command_lock);
            return;
        }
        if (!strstr(text, "BBBBBBBB")) HAL_vUartSend(HAL_UART_MAIN_BOARD, data, len, 1000);
    }
    for (uint32_t i = 0; i < len; ++i) {
        if (data[i] == '\r' || data[i] == '\n' || data[i] == 0) continue;
        if (used + 1 >= sizeof(command)) { used = 0; continue; }
        command[used++] = data[i];
    }
    command[used] = 0;
    xSemaphoreGive(command_lock);
}

/**
 * @brief 每 20 ms 检查命令，接收静默至少 100 ms 后尝试提交。
 * @param[in] arg 定时参数，未使用。
 * @note 不等待互斥锁，接收任务正在处理时留待下一轮检查。
 */
static void factory_poll(void *arg)
{
    (void)arg;
    /* Frame completion is independent of UART DMA chunk boundaries. */
    if (xSemaphoreTake(command_lock, 0) != pdTRUE) return;
    if (used && HAL_ucTickTimeout(recv_tick, 100)) {
        factory_commit();
        used = 0;
    }
    xSemaphoreGive(command_lock);
}

/**
 * @brief 向主板和 LOG UART 发送工厂提示，并恢复回环后的慢闪状态。
 * @param[in] arg 定时参数，未使用。
 * @note 使用 sizeof(prompt) 发送，保留参考工厂协议中的末尾 NUL。
 */
static void factory_prompt(void *arg)
{
    (void)arg;
    const char prompt[] = FACTORY_PROMPT;
    HAL_vUartSend(HAL_UART_MAIN_BOARD, (uint8_t *)prompt, sizeof(prompt), 1000);
    HAL_vUartSend(HAL_UART_DEBUG, (uint8_t *)prompt, sizeof(prompt), 1000);
    if (loopback) {
        loopback = 0;
        APP_vLedSstSta(LED_GREEN, LED_STA_SLOW_FLASH);
        APP_vLedSstSta(LED_RED, LED_STA_SLOW_FLASH);
    }
    set_led_status(LED_STATUS_NO_DEVID);
}

/**
 * @brief 建立工厂接收互斥锁，注册烧号回调和 4 秒提示/20 ms 提交定时器。
 * @pre 主板 UART 和软件定时循环已由启动流程准备。
 * @note 创建互斥锁失败时直接返回；仅由启动模式选择流程调用一次。
 */
void APP_vTestInit(void)
{
    command_lock = xSemaphoreCreateMutex();
    if (!command_lock) return;
    set_led_status(LED_STATUS_NO_DEVID);
    APP_vLedSstSta(LED_GREEN, LED_STA_SLOW_FLASH);
    APP_vLedSstSta(LED_RED, LED_STA_SLOW_FLASH);
    HAL_vUartRecvProc(HAL_UART_MAIN_BOARD, factory_recv);
    static struct HalSoftTimer prompt, poll;
    HAL_vSoftTimeInit(&prompt, factory_prompt, 4000, NULL);
    HAL_vSoftTimeInit(&poll, factory_poll, 20, NULL);
}

/** @brief 保留旧工厂入口名称，行为等同 APP_vTestInit()。 */
void APP_vFactoryInit(void) { APP_vTestInit(); }
