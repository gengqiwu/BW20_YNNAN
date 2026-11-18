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

void APP_vFactoryInit(void) { APP_vTestInit(); }
