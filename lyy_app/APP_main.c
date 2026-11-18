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
#include "thing_init.h"

static void APP_vSoftTimeTask(void *arg)
{
    (void)arg;
    for (;;) {
        HAL_vSoftTimeLoop();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void APP_vWatchdogTask(void *arg)
{
    (void)arg;
    HAL_vIWatchdogRefresh();
}

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

    /* All bill, machine and screen traffic is routed through STM32. */
    lyy_init_device();
    if (WIFI_AP == APP_xGetParamWifiMode()) {
        WIFI_INFO_T wifi = {0};
        wifi.xMode = WIFI_STA;
        strcpy(wifi.pcSsid, LYY_WIFI_SSID);
        strcpy(wifi.pcPwd, LYY_WIFI_PWD);
        APP_lParamSetWifiInfo(&wifi);
        APP_prvDetectMachtineLvl();
        set_led_status(APP_ulGetMachtineLvl() ? LED_STATUS_WIFI_SMART : LED_STATUS_WIFI_AP);
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

void app_example(void)
{
    APP_vMainInit();
    xTaskCreate(APP_vSoftTimeTask, "SOFT_TIME_THREAD", 8 * 1024 / sizeof(portSTACK_TYPE), NULL, 5, NULL);
    HAL_vIWatchdong(10000);
    static struct HalSoftTimer watchdog;
    HAL_vSoftTimeInit(&watchdog, APP_vWatchdogTask, 4000, NULL);
}
