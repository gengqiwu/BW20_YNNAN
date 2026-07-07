#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "USER_Config.h"

#include "HAL_Log.h"
#include "HAL_Tls.h"
#include "HAL_Wifi.h"
#include "HAL_System.h"


#include "APP_Led.h"
#include "APP_Guide.h"
#include "APP_Param.h"
#include "APP_NetWork.h"

#include "thing_mqtt.h"


typedef struct {

    uint8_t             ucCloseTls;
    APP_CLIENT_STA_T    xClientSta;
    WIFI_WORK_STA_T     xWifiSta;
    hal_tls_t           *pxTls;

}APP_NET_INFO_T;


static APP_NET_INFO_T g_xAppNetInfo;


APP_CLIENT_STA_T APP_pxGetClientSta(void)
{
    return g_xAppNetInfo.xClientSta;
}


static void APP_prvClientCloseSocket(void)
{

    if(g_xAppNetInfo.pxTls)  {
        
        HAL_vTlsClose(g_xAppNetInfo.pxTls);
        g_xAppNetInfo.pxTls = NULL;

        vTaskDelay(200  / portTICK_PERIOD_MS);
    }

}


int APP_lClientWrite(char *pcBuf, int lLen)
{ 

    if  (NULL == g_xAppNetInfo.pxTls || 
        g_xAppNetInfo.xClientSta != CLIENT_CONED)  {

        return 0;
    }

    return  HAL_lTlsWrite(  g_xAppNetInfo.pxTls,  
                            pcBuf, 
                            lLen, 
                            1000);
}



void APP_prvWifiWorkSta(WIFI_WORK_STA_T xSta)
{

    if(xSta == g_xAppNetInfo.xWifiSta) {

        return ;
    }

    switch(xSta)
    {

        case WIFI_WORK_IDLE     :{

#if(CFG_USER_SOC == CFG_SOC_RTL8711)

            APP_vLedSstSta(LED_GREEN,   LED_STA_OFF);
            APP_vLedSstSta(LED_RED,     LED_STA_SLOW_FLASH);

#endif

            LYY_LOGI(">>> WIFI_WORK_IDLE ");

        }break;

        case WIFI_STA_CONNING  :{

            APP_vLedSstSta(LED_GREEN,   LED_STA_OFF);
            APP_vLedSstSta(LED_RED,     LED_STA_SLOW_FLASH);
            
            LYY_LOGI(">>> WIFI_WORK_CONNING");

        }break;

        case WIFI_STA_CONNED   :{

            APP_vLedSstSta(LED_GREEN,   LED_STA_ON);
            APP_vLedSstSta(LED_RED,     LED_STA_SLOW_FLASH);

            LYY_LOGI(">>> WIFI_WORK_CONNED");

        }break;

        default : ;

    }

    g_xAppNetInfo.xWifiSta = xSta;
    
}


static void APP_prvClientTlsTask(void *pvArg)
{
    
#define CLIENT_TASK_TIME            (30U)

    (void)pvArg;

    uint16_t wifi_connect_count = 0;
    uint16_t wifi_connect_flag  = 0;


    WIFI_INFO_T xWifiInfo_default = {CFG_WIFI_SSID, CFG_WIFI_PWD, WIFI_STA};
    WIFI_INFO_T xWifiInfo = { 0x00,  };

    xWifiInfo.xMode = WIFI_STA;

#ifndef CFG_TEST_MODE

    strcpy(xWifiInfo.pcSsid,    APP_pcGetParamWifiSsid());
    strcpy(xWifiInfo.pcPwd,     APP_pcGetParamWifiPwd());

#else

    strcpy(xWifiInfo.pcSsid,    CFG_WIFI_TEST_SSID);
    strcpy(xWifiInfo.pcPwd,     CFG_WIFI_TEST_PWD);

#endif

    HAL_vWifiInit(&xWifiInfo, APP_prvWifiWorkSta);

    int lRet                = 0x01;

#if(CFG_USER_SOC == CFG_SOC_RTL8711)

    static int lWifiSta     = WIFI_WORK_IDLE;

#endif


    for(; ; ) {

        vTaskDelay(CLIENT_TASK_TIME  / portTICK_PERIOD_MS);

        if(g_xAppNetInfo.xWifiSta != WIFI_STA_CONNED) {

            g_xAppNetInfo.xClientSta =  CLIENT_DIS_CON;

#if(CFG_USER_SOC == CFG_SOC_RTL8711)

            lWifiSta     =  g_xAppNetInfo.xWifiSta;

#endif
            wifi_connect_count++;
            if (wifi_connect_count >= (10000 / CLIENT_TASK_TIME))
            {
                wifi_connect_count = 0;
                if (wifi_connect_flag==1)
                {
                    wifi_connect_flag = 0;
                    HAL_wifista_again_connect_wifi(&xWifiInfo);
                    LYY_LOGI(">>> HAL_wifista_connect_flash_wifi");
                }
                else
                {
                    wifi_connect_flag = 1;
                    HAL_wifista_again_connect_wifi(&xWifiInfo_default);
                    LYY_LOGI(">>> HAL_wifista_connect_default_wifi");
                }
            }

            continue;
        }

#if(CFG_USER_SOC == CFG_SOC_RTL8711)

        if(lWifiSta != g_xAppNetInfo.xWifiSta) {

            if(WIFI_STA_CONNED == g_xAppNetInfo.xWifiSta) {

                HAL_vWifiLwipDhcpInit();
            }

            lWifiSta = g_xAppNetInfo.xWifiSta;
        }

#endif


#ifndef CFG_PLUSE
        /* 资源不足 */
        APP_vGuideLoop();
#endif

        switch(g_xAppNetInfo.xClientSta)
        {

            case CLIENT_DIS_CON :{

                lyy_mqtt_disconnected();
                APP_prvClientCloseSocket();

                g_xAppNetInfo.pxTls = HAL_vTlsCreat();

                if(!g_xAppNetInfo.pxTls) {

                    continue;
                }

                /* 阻塞连接 */
                lRet = HAL_lTlsEstablish(   g_xAppNetInfo.pxTls, 
                                            APP_pcGetSerUrl() , 
                                            strlen(APP_pcGetSerUrl()), 
                                            APP_usGetSerPort(),  
                                            1000 * 10);

                if(lRet == 0x01 && g_xAppNetInfo.pxTls != NULL) {

                    lyy_mqtt_start();
                    
                    g_xAppNetInfo.xClientSta = CLIENT_CONED;

                    LYY_LOGD(">>>> CONNECT SUCCESS");

                }

                //todo... (连接超时重启设备)

            }break;

            case CLIENT_CONNING :{

            }break;

            case CLIENT_CONED   :{

                if( NULL == g_xAppNetInfo.pxTls) {

                    continue ;
                }

                char pcBuf[1024 * 4] = {0x00, };

                lRet = HAL_lTlsRead(g_xAppNetInfo.pxTls, 
                                    pcBuf, 
                                    sizeof(pcBuf), 
                                    1000);

                if(lRet > 0)  {

                    lyy_mqtt_overlap(   (unsigned char *)pcBuf,
                                        lRet, 
                                        lyy_mqtt_packet_parse);
                }

                if(lRet < 0) {

                    g_xAppNetInfo.xClientSta =  CLIENT_DIS_CON;
                    LYY_LOGE("===========  sock disconnect ===========");
                }
                
            }break;

        }
    
    }

}


static void APP_prvMqttTask(void *arg)
{

#define MQTT_CLIENT_RUN_TIME             (100U)

    (void)arg;

    for( ; ; )  {

        lyy_mqtt_status_task(arg);
        vTaskDelay(MQTT_CLIENT_RUN_TIME  / portTICK_PERIOD_MS);
    }

}


void APP_vNetWorkInit(void)
{

#define CLIENT_THREAD_NAME          "CLIENT_THREAD"
#define CLIENT_THREAD_STACK         (1024 * 18 / sizeof(portSTACK_TYPE))

    xTaskCreate(    APP_prvClientTlsTask, 
                    CLIENT_THREAD_NAME, 
                    CLIENT_THREAD_STACK, 
                    NULL, 
                    9,
                    NULL);

#define MQTT_THREAD_NAME             "MQTT_THREAD"
#define MQTT_THREAD_STACK            (1024 * 4 / sizeof(portSTACK_TYPE))


    xTaskCreate(    APP_prvMqttTask, 
                    MQTT_THREAD_NAME, 
                    MQTT_THREAD_STACK, 
                    NULL, 
                    8,
                    NULL); 

}

