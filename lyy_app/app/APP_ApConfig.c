/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-10-19 17:18:47
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-04 16:03:41
 * @FilePath: \ESP8266_RTOS_SDK\lyy_app\main\app\APP_ApConfig.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "FreeRTOS.h"
#include "task.h"

#include "cJSON.h"

#include "HAL_Log.h"
#include "HAL_Udp.h"
#include "HAL_Wifi.h"
#include "HAL_System.h"


#include "APP_Param.h"


typedef struct {

    int                 lSockFd;
    HAL_NETWORK_ADDR_T  xRemote;
}AP_CONFIG_T;

#define UDP_SERVER_PORT                 (8266U)

static AP_CONFIG_T xApConfig;


static void APP_prvWifiApSta(WIFI_WORK_STA_T xSta)
{
    LYY_LOGI(">>> wifi ap static : [%d]", xSta);
}


static void APP_prvUdpSendSuccess(void)
{

    cJSON *pxRoot = cJSON_CreateObject();

    if(NULL == pxRoot) {

        LYY_LOGE("### pxRoot err");
        return ;
    }

    cJSON_AddNumberToObject(pxRoot, "cmdType", 0x02);    

    char *pcData = cJSON_PrintUnformatted(pxRoot);

    HAL_lUdpSendTo( xApConfig.lSockFd, 
                    &xApConfig.xRemote, 
                    (uint8_t *)pcData, 
                    strlen(pcData), 
                    1000);

    free(pcData);

    cJSON_Delete(pxRoot);   

}


static void APP_prvUdpRecvDataProc(uint8_t *pucData, uint32_t ulDataLen)
{

    (void)ulDataLen;

    cJSON *pxJsonData = NULL;

    pxJsonData = cJSON_Parse((const char *)pucData);

    if(NULL == pxJsonData)  {

        LYY_LOGE("### pxJsonData NULL");
        return ;
    }

    WIFI_INFO_T pcWifiInfo = {0};

    cJSON *pxWifiSsid = cJSON_GetObjectItem(pxJsonData, "ssid"    );
    cJSON *pxWifiPwd  = cJSON_GetObjectItem(pxJsonData, "password");

    if(NULL == pxWifiSsid || NULL == pxWifiPwd ||
       pxWifiSsid->type != cJSON_String || pxWifiPwd->type != cJSON_String ||
       !pxWifiSsid->valuestring || !pxWifiPwd->valuestring ||
       !pxWifiSsid->valuestring[0] || strlen(pxWifiSsid->valuestring) > 32 ||
       strlen(pxWifiPwd->valuestring) >= sizeof(pcWifiInfo.pcPwd)) {

        goto cjson_exit;
    }


    pcWifiInfo.xMode = WIFI_STA;

    strcpy(pcWifiInfo.pcSsid,   pxWifiSsid->valuestring);
    strcpy(pcWifiInfo.pcPwd,    pxWifiPwd->valuestring);

    LYY_LOGI(">>> ssid : [%s]", pcWifiInfo.pcSsid);

    APP_lParamSetWifiInfo(&pcWifiInfo);

    APP_prvUdpSendSuccess();

    HAL_vReset("ap success", 100);

cjson_exit:

    cJSON_Delete(pxJsonData);

}


static void APP_prvApTask(void *pvArg)
{

    (void)pvArg;

#define AP_CONFIG_TASK_TIME        (100U)
    
    int         lRet;
    uint8_t     pucData[256]    = {0x00,  };
    WIFI_INFO_T xWifiInfo       = {0x00,  };

    xWifiInfo.xMode = WIFI_AP;

    snprintf(  xWifiInfo.pcSsid, 
                33,
                "lyy_%s", 
                (const char *)APP_pcGetParamDevId());


    strcpy(xWifiInfo.pcPwd,    "lyy16888");

    HAL_vWifiInit(&xWifiInfo, APP_prvWifiApSta);

    xApConfig.lSockFd = HAL_lUdpServerCreate(NULL, UDP_SERVER_PORT);

    if(HAL_UDP_SOCK_ERR == xApConfig.lSockFd) {

        LYY_LOGE("### udp sock err");
        vTaskDelete(NULL);
        return;
    }

    for( ; ; ) {

        lRet = HAL_lUdpRecvFrom(xApConfig.lSockFd, 
                                &xApConfig.xRemote, 
                                pucData, 
                                sizeof(pucData) - 1,
                                1000);

        if(lRet > 0x00) {
            pucData[lRet] = '\0';

            LYY_LOGD(">>> udp recv pucData : [%s]", pucData);
            APP_prvUdpRecvDataProc(pucData, lRet);
        }

        vTaskDelay(AP_CONFIG_TASK_TIME  / portTICK_PERIOD_MS);

    }

}



void APP_vApConfigSet(void *pvArg)
{

    (void)pvArg;

    APP_vSetParamWifiMode(WIFI_AP);

    HAL_vReset("WIFI_AP", 500);

}



void APP_vApConfigInit(void)
{

#define AP_THREAD_NAME          "AP_THREAD"
#define AP_THREAD_STACK         (1024 * 4 / sizeof(portSTACK_TYPE))


    xTaskCreate(    APP_prvApTask, 
                    AP_THREAD_NAME, 
                    AP_THREAD_STACK, 
                    NULL, 
                    5,
                    NULL);
}





