/**
 * @file APP_DevAp.c
 * @brief 连接预置配网热点，通过 UDP 广播发现设备并接收新 Wi-Fi 凭据。
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

    uint8_t             ulWiftSta;
    int                 lSockFd;
    HAL_NETWORK_ADDR_T  xRemote;
    HAL_NETWORK_ADDR_T  xBroadcast;
}AP_CONFIG_T;

#define UDP_SERVER_PORT                 (8888U)

static AP_CONFIG_T xApConfig;
static void APP_prvUdpSendHandShake(void *arg);


static void APP_prvDevWifiWorkSta(WIFI_WORK_STA_T xSta)
{

    if(WIFI_STA_CONNED == xSta) {

        xApConfig.ulWiftSta = 0x01;
        LYY_LOGI(">>> WIFI_WORK_CONNED");
        
    }else{

        xApConfig.ulWiftSta = 0x00;
        LYY_LOGI(">>> WIFI_WORK_NOCONNED");
    }

}

static void APP_prvUdpSendSuccess(void)
{

    cJSON *pxRoot = cJSON_CreateObject();

    if(NULL == pxRoot)
    {
        LYY_LOGE("### pxRoot err");
        return ;
    }

    cJSON_AddStringToObject(pxRoot, "deviceId", APP_pcGetParamDevId());    
    cJSON_AddStringToObject(pxRoot, "type",     "bye");   

    char *pcData = cJSON_PrintUnformatted(pxRoot);

    HAL_lUdpSendTo( xApConfig.lSockFd, 
                    &xApConfig.xRemote, 
                    (uint8_t *)pcData, 
                    strlen(pcData), 
                    1000);

    free(pcData);
    cJSON_Delete(pxRoot);   

}


/**
 * @brief 在 Wi-Fi 已连接且广播地址非空时广播设备 hello 信息。
 * @param[in] pvArg 软件定时器参数，未使用。
 * @note 可由工作任务或周期定时器调用；此处不提供并发互斥。
 */
static void APP_prvUdpSendHandShake(void *pvArg)
{

    (void)pvArg;

    if(xApConfig.ulWiftSta != 0x01 || !xApConfig.xBroadcast.pucAddr[0]) {
        return ;
    }

    cJSON *pxRoot = cJSON_CreateObject();

    if(NULL == pxRoot)
    {
        LYY_LOGE("### pxRoot err");
        return ;
    }

    cJSON_AddStringToObject(pxRoot, "deviceId", APP_pcGetParamDevId());    
    cJSON_AddStringToObject(pxRoot, "type",     "hello");   

    char *pcData = cJSON_PrintUnformatted(pxRoot);

    HAL_lUdpSendTo( xApConfig.lSockFd, 
                    &xApConfig.xBroadcast, 
                    (uint8_t *)pcData, 
                    strlen(pcData), 
                    1000);

    free(pcData);
    cJSON_Delete(pxRoot);   

}


/**
 * @brief 校验并保存 UDP JSON 中的站点凭据，回复 bye 后请求重启。
 * @param[in] pucData 调用方已补 NUL 结尾的接收数据，不在此释放。
 * @param[in] ulDataLen 接收字节数；当前未用于限制 JSON 解析范围。
 * @note SSID 必须为 1～32 字节字符串；密码可为空但不能占满密码数组。
 *       保存接口返回值未检查，回复不代表已确认持久化成功。
 */
static void APP_prvUdpRecvDataProc(uint8_t *pucData, uint32_t ulDataLen)
{

    (void)ulDataLen;

    cJSON *pxJsonData = NULL;

    pxJsonData = cJSON_Parse((const char *)pucData);

    if(NULL == pxJsonData) 
    {

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
       strlen(pxWifiPwd->valuestring) >= sizeof(pcWifiInfo.pcPwd))
    {
        goto cjson_exit;
    }


    pcWifiInfo.xMode = WIFI_STA;

    strcpy(pcWifiInfo.pcSsid,   pxWifiSsid->valuestring);
    strcpy(pcWifiInfo.pcPwd,    pxWifiPwd->valuestring);

    LYY_LOGI(">>> ssid : [%s]", pcWifiInfo.pcSsid);

    APP_lParamSetWifiInfo(&pcWifiInfo);

    APP_prvUdpSendSuccess();

    HAL_vReset("ap success", 1000);

cjson_exit:

    cJSON_Delete(pxJsonData);

}


/**
 * @brief 连接预置热点，在 8684 端口接收配网数据并更新广播地址。
 * @param[in] pvArg 任务参数，未使用。
 * @note 联网后向 8888 端口广播；接收最多 255 字节并补 NUL 后解析。
 *       创建 UDP 服务失败会删除当前任务，否则循环运行。
 * @warning 调试日志输出原始 UDP 内容，可能包含配网密码。
 */
static void APP_prvDevApTask(void *pvArg)
{

    (void)pvArg;

#define AP_CONFIG_TASK_TIME        (100U)
#define APP_DEV_AP_SSID         "starthing"
#define APP_DEV_AP_PWD          "168168168"
    
    int                 lRet;
    uint8_t             pucData[256] = {0x00};
    WIFI_INFO_T         xWifiInfo = {   APP_DEV_AP_SSID,  
                                        APP_DEV_AP_PWD, 
                                        WIFI_STA};
    static uint32_t     ulWifiSta;

    HAL_vWifiInit(&xWifiInfo, APP_prvDevWifiWorkSta);

    xApConfig.lSockFd = HAL_lUdpServerCreate(NULL, 8684);

    if(HAL_UDP_SOCK_ERR == xApConfig.lSockFd) {
        
        LYY_LOGE("### udp sock err");
        vTaskDelete(NULL);
        return;
    }


    for( ; ; ) {

        vTaskDelay(AP_CONFIG_TASK_TIME  / portTICK_PERIOD_MS);

        if(ulWifiSta != xApConfig.ulWiftSta) {

            ulWifiSta = xApConfig.ulWiftSta;

            if(ulWifiSta) {

#if(CFG_USER_SOC == CFG_SOC_RTL8711)

                HAL_vWifiLwipDhcpInit();

#endif
                uint32_t ulBroadcastAddr    = HAL_ulGetBroadcastAddr();

                xApConfig.xBroadcast.usPort = UDP_SERVER_PORT;

                sprintf((char *)xApConfig.xBroadcast.pucAddr, "%u.%u.%u.%u",
                            (uint8_t)(ulBroadcastAddr >> 0),
                            (uint8_t)(ulBroadcastAddr >> 8),
                            (uint8_t)(ulBroadcastAddr >> 16),
                            (uint8_t)(ulBroadcastAddr >> 24));

                LYY_LOGI("Broadcast Address: %s",    xApConfig.xBroadcast.pucAddr);

                APP_prvUdpSendHandShake(NULL);
            }

        }

        if(!ulWifiSta) {

            continue ;
        }

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

        

    }
}


void APP_vDevApInit(void)
{

#define APP_DEVAP_THREAD_NAME          "DEVAP_THREAD"
#define APP_DEVAP_THREAD_STACK         (1024 * 4 / sizeof(portSTACK_TYPE))

    xTaskCreate(    APP_prvDevApTask, 
                    APP_DEVAP_THREAD_NAME, 
                    APP_DEVAP_THREAD_STACK, 
                    NULL, 
                    5,
                    NULL);

#define APP_SEND_AP_INFO_TIME       (3 * 1000)

   static struct HalSoftTimer AppSendTask;

    HAL_vSoftTimeInit(&AppSendTask, 
                      APP_prvUdpSendHandShake, 
                      APP_SEND_AP_INFO_TIME,
                      NULL);

}












