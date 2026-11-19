/**
 * @file APP_main.c
 * @brief BW20 纸钞盒启动入口，按烧号、配网和正常业务选择初始化路径。
 * @note 现金采集、机台输出和屏幕物理收发由 STM32 执行，BW20 负责网络与协议代理。
 */
#include <stdio.h>
#include <string.h>
#include "USER_Config.h"
#include "HAL_Log.h"
#include "HAL_Gpio.h"
#include "HAL_Uart.h"
#include "HAL_Flash.h"
#include "HAL_System.h"
#include "APP_Led.h"
#include "APP_Event.h"
#include "APP_Param.h"
#include "APP_DevAp.h"
#include "APP_Timer.h"
#include "APP_Detect.h"
#include "APP_Factory.h"
#include "APP_NetWork.h"
#include "APP_ApConfig.h"
#include "APP_DrillingMode.h"
#include "lyy_tjc.h"
#include "lyy_device.h"
#include "lyy_zcj.h"
#include "thing_init.h"

/**
 * @brief 每 10 ms 调度一次已注册的软件定时回调。
 * @param[in] arg 任务参数，未使用。
 * @note 配网启动补发放在此任务中执行，串口发送使用的 RTOS API 要求调度器已运行。
 * @note 回调共享当前任务上下文，不应在回调中长时间阻塞。
 */
static void APP_vSoftTimeTask(void *arg)
{
    (void)arg;
    uint8_t status = get_led_status();
    if (status == LED_STATUS_WIFI_AP || status == LED_STATUS_WIFI_SMART) {
        zcj_notify_provisioning_led(status);
    }

    for (;;) {
        HAL_vSoftTimeLoop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * @brief 公共软件定时器中的看门狗刷新回调。
 * @param[in] arg 定时参数，未使用。
 */
static void APP_vWatchdogTask(void *arg)
{
    (void)arg;
    HAL_vIWatchdogRefresh();
}

/**
 * @brief 初始化持久参数、固定速率的 STM32 链路并选择运行模式。
 * @details 无有效设备号时仅启动工厂烧号；配网分支仍初始化 STM32 协议，
 *          以便响应设备配置轮询；正常分支再启动支付队列、联网及屏幕业务。
 * @note 机台波特率是 STM32 外设参数，不改变模块间固定 115200 的物理链路。
 */
void APP_vMainInit(void)
{
    HAL_vFlashInit();
    int ret = APP_lParamInit();
    HAL_vUartInit(HAL_UART_DEBUG, 115200);
    HAL_vUartInit(HAL_UART_MAIN_BOARD, 115200);
    HAL_vGpioRegister();
    APP_vLedInit();
    set_led_status(LED_STATUS_START_SYSTEM);

    if (ret != LYY_SUCCESS || !strlen(APP_pcGetParamDevId())) {
        APP_vTestInit();
        return;
    }

    const uint8_t provisioning = (APP_xGetParamWifiMode() == WIFI_AP);
    if (provisioning) {
        APP_prvDetectMachtineLvl();
        set_led_status(APP_ulGetMachtineLvl() ? LED_STATUS_WIFI_SMART : LED_STATUS_WIFI_AP);
    }

    /* Set the provisioning status before wiring up the D6/0B reply handler. */
    lyy_init_device();
    if (provisioning) {
        WIFI_INFO_T wifi = {0};
        wifi.xMode = WIFI_STA;
        strcpy(wifi.pcSsid, LYY_WIFI_SSID);
        strcpy(wifi.pcPwd, LYY_WIFI_PWD);
        APP_lParamSetWifiInfo(&wifi);
        if (APP_ulGetMachtineLvl()) APP_vDevApInit();
        else APP_vApConfigInit();
        APP_vLedSstSta(LED_GREEN, APP_ulGetMachtineLvl() ? LED_STA_OFF : LED_STA_ON);
        APP_vLedSstSta(LED_RED, APP_ulGetMachtineLvl() ? LED_STA_ON : LED_STA_OFF);
        return;
    }

    APP_vEvenInit();
    APP_vEventTaskCreate();
    APP_vNetWorkInit();
    APP_vDrillingModeInit();
    APP_vDeteclRegisterFunc(APP_NETWORK_DETECT, APP_vApConfigSet);
    APP_Timer1msInit();
    lyy_init_tjc();
    lyy_mqtt_model_init();
}

/**
 * @brief SDK 应用入口，启动业务、公共软定时任务和硬件看门狗。
 * @note 看门狗超时 10 秒，软定时回调每 4 秒刷新一次；工厂与配网模式也使用此任务。
 */
void app_example(void)
{
    APP_vMainInit();
    xTaskCreate(APP_vSoftTimeTask, "SOFT_TIME_THREAD", 8 * 1024 / sizeof(portSTACK_TYPE), NULL, 5, NULL);
    HAL_vIWatchdong(10000);
    static struct HalSoftTimer watchdog;
    HAL_vSoftTimeInit(&watchdog, APP_vWatchdogTask, 4000, NULL);
}
