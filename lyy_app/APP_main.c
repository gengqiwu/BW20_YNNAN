/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-11-22 10:46:23
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2025-01-12 14:48:35
 * @FilePath: \ameba-rtos\lyy_app\lyy_main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "USER_Config.h"

#include "HAL_Log.h"
#include "HAL_GPIO.h"
#include "HAL_Uart.h"
#include "HAL_Flash.h"
#include "HAL_System.h"

#include "APP_Led.h"
#include "APP_Event.h"
#include "APP_Param.h"
#include "APP_DevAp.h"
#include "APP_Timer.h"
#include "APP_Pluse.h"
#include "APP_Detect.h"
#include "APP_Factory.h"
#include "APP_NetWork.h"
#include "APP_ApConfig.h"
#include "APP_DrillingMode.h"


#include "lyy_tjc.h"
#include "thing_init.h"



#ifdef CFG_NO_OS

void APP_vSoftTimeTask(void *arg)
{

    (void)arg;
 
    while (1) {

        HAL_vSoftTimeLoop();
        
        vTaskDelay(10 / portTICK_PERIOD_MS);

    }   

}

#endif


#if(CFG_USER_SOC == CFG_SOC_RTL8711)

static void APP_PrvIWatchDongTask(void *pvArg)
{

    (void)pvArg;
    HAL_vIWatchdogRefresh();

}

#endif


void APP_vMainInit(void)
{

    HAL_vGpioRegister();


    HAL_vFlashInit();

    int lRet = APP_lParamInit();

    if(lRet == LYY_SUCCESS && !strlen(APP_pcGetParamDevId())) {

        lRet = LYY_READ_PARAM_FAIL;
    }

    HAL_vUartInit(HAL_UART_MAIN_BOARD, LYY_READ_PARAM_FAIL == lRet ? 115200 : APP_ulGetParamBaudRate());
    // while (1)
    // {
    //     vTaskDelay(10 / portTICK_PERIOD_MS);
    // }
    APP_vLedInit();
    
    LYY_LOGD("APP_vLedInit");
    if(LYY_READ_PARAM_FAIL == lRet)  {

        APP_vFactoryInit();
        return ;
    }
    
    LYY_LOGD("APP_vFactoryInit");
    /* 进入配网模式 */
    if(WIFI_AP == APP_xGetParamWifiMode()) {

        /* 客户配网失败， 重启后连回默认wifi */
        WIFI_INFO_T pcWifiInfo;

        pcWifiInfo.xMode = WIFI_STA;

        strcpy(pcWifiInfo.pcSsid,   LYY_WIFI_SSID);
        strcpy(pcWifiInfo.pcPwd,    LYY_WIFI_PWD);

        APP_lParamSetWifiInfo(&pcWifiInfo);

        APP_prvDetectMachtineLvl();

        APP_ulGetMachtineLvl() ? APP_vDevApInit() : APP_vApConfigInit();

        APP_vLedSstSta(LED_GREEN,   APP_ulGetMachtineLvl() ? LED_STA_OFF : LED_STA_ON);
        APP_vLedSstSta(LED_RED,     APP_ulGetMachtineLvl() ? LED_STA_ON  : LED_STA_OFF);

        LYY_LOGD("WIFI AP..... %s" , APP_ulGetMachtineLvl() ? "[DEV_AP]" : "[HARD_AP]");

        return ;
    }

    APP_vEvenInit();
    LYY_LOGD("APP_vEvenInit");
    APP_vEventTaskCreate();
    
    APP_vDetectInit();
    APP_vNetWorkInit();
    APP_vDrillingModeInit();
    APP_Timer1msInit();
    LYY_LOGD("APP_Timer1msInit");

        /* 外设触发 */
    APP_vDeteclRegisterFunc(APP_NETWORK_DETECT, APP_vApConfigSet);
    APP_vDeteclRegisterFunc(APP_INPUT_GIFT,     APP_vGiftOut);
    APP_vDeteclRegisterFunc(APP_INPUT_COIN,     APP_vCoinIn);

    /* 平台业务 */
    lyy_init_tjc();
    lyy_mqtt_model_init();
    LYY_LOGD("lyy_mqtt_model_init");
}

void app_example(void)
{

    // HAL_vGpioRegister();

    // while (1)
    // {
    //     vTaskDelay(10 / portTICK_PERIOD_MS);
    // }
    APP_vMainInit();

#ifdef CFG_NO_OS

#define SOFT_TIME_THREAD_NAME          "SOFT_TIME_THREAD"
#define SOFT_TIME_THREAD_STACK         (8 * 1024 / sizeof(portSTACK_TYPE))

    xTaskCreate(    APP_vSoftTimeTask, 
                    SOFT_TIME_THREAD_NAME, 
                    SOFT_TIME_THREAD_STACK, 
                    NULL, 
                    5,
                    NULL);

#endif

#if(CFG_USER_SOC == CFG_SOC_RTL8711)

#define APP_WACTHDONG_TIMEOUT           (10 * 1000U)
#define APP_WACTHDONG_REFRESH           (4 * 1000U)

    HAL_vIWatchdong(APP_WACTHDONG_TIMEOUT);

    static struct HalSoftTimer AppWacthDongTask;

    HAL_vSoftTimeInit(&AppWacthDongTask, 
                      APP_PrvIWatchDongTask, 
                      APP_WACTHDONG_REFRESH,
                      NULL);

#endif


}

