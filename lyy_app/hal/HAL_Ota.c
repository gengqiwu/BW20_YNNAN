#include <stdio.h>
#include <stdarg.h>
#include <string.h>


#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_System.h"
#include "HAL_Flash.h"

#include "sys_api.h"
#include "ameba_ota.h"

enum {

    eOtaIdle,
    eOtaRunnig,
};

typedef struct {

    uint8_t ucOtaSta;
}HalOtaInfo_t;


static HalOtaInfo_t g_halOtaInfo;

void HAL_prvOtaThread(void *pvArg)
{


    int         lRet;
	ota_context *pxCtx = NULL;

    uint16_t ulPort;
    char    *pcPath;
    char    *pcHostname;
    char     pcHostNameStr[256]     = {0x00, };
    char     pcPathStr[512]         = {0x00, };

    if (strncmp((const char *)pvArg, "http://", 7) == 0) {

        ulPort = 80;
        pcHostname = (char *)pvArg + 7;

    } else if (strncmp((const char *)pvArg, "https://", 8) == 0) {

        ulPort = 443;
        pcHostname = (char *)pvArg + 8;

    } else {

        LYY_LOGE("Invalid URL format\n");
        return ;
    }

    pcPath = strchr(pcHostname, '/');

    if (pcPath == NULL) {

        LYY_LOGE("Invalid URL format\n");
        return ;
    }

    *pcPath = '\0';

    if(strlen(pcHostname) > sizeof(pcHostNameStr)) {

        LYY_LOGE("### pcHostname len err ");
        return ;
    }

    strcpy(pcHostNameStr, pcHostname);                                                                                      

    *pcPath = '/';

    if(strlen(pcPath) > sizeof(pcPathStr)) {

        LYY_LOGE("### pcPath len err ");
        return ;
    }

    strcpy(pcPathStr, pcPath + 1);

    LYY_LOGD("Hostname: %s\n",  pcHostNameStr);
    LYY_LOGD("Port: %d\n",      ulPort);
    LYY_LOGD("Path: %s\n",      pcPathStr);

	pxCtx = (ota_context *)pvPortMalloc(sizeof(ota_context));

	if (pxCtx == NULL) {

		LYY_LOGD("### ctx malloc failed");
		goto ota_exit;
	}

    g_halOtaInfo.ucOtaSta = eOtaRunnig;

	lRet = ota_update_init( pxCtx, 
                            (char *)pcHostNameStr, 
                            ulPort, 
                            (char *)pcPathStr, 
                            ulPort == 80 ? OTA_HTTP : OTA_HTTPS);
	if (lRet != 0) {

		LYY_LOGE(" ota_update_init failed");
		goto ota_exit;

	}

	lRet = ota_update_start(pxCtx);

	LYY_LOGD(">>>>  Update task exit");

	if (!lRet) {

        sys_clear_ota_signature();
        HAL_vReset("Ready to reboot", 50);
	}

ota_exit:

	ota_update_deinit(pxCtx);

	if (pxCtx) {

		vPortFree(pxCtx);
	}

    g_halOtaInfo.ucOtaSta = eOtaIdle;

    vTaskDelete(NULL);

}


void HAL_vOtaHttp(const char *pcUrl, vOtaStaFunc xOtaStaFunc)
{


    (void)xOtaStaFunc;
    
    if(g_halOtaInfo.ucOtaSta != eOtaIdle) {

        LYY_LOGE("### OTA NO IDLE");
        return ;
    }

    
    LYY_LOGD(">>>> OTA INIT");

#define OTA_THREAD_NAME          "OTA_THREAD"
#define OTA_THREAD_STACK         (1024 * 16 / sizeof(portSTACK_TYPE))

    xTaskCreate(    HAL_prvOtaThread, 
                    OTA_THREAD_NAME, 
                    OTA_THREAD_STACK, 
                    (void *)pcUrl, 
                    10,
                    NULL);


}



