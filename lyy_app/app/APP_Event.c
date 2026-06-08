#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "HAL_Log.h"

#include "APP_Event.h"

#include "ringbuf.h"


static APP_EVENT_RINGBUF_T    xEventQueue[eEventMax];


static void APP_prvEventRecv(uint8_t ucType)
{

	if(RingBufEmpty(&xEventQueue[ucType].xEventRing)) {

		return ;
	}

    APP_EVENT_T xEvent;

    RingBufReadOne(&xEventQueue[ucType].xEventRing, (void *)&xEvent);

    LYY_LOGD(">>> xEvent[%d], Get Success ", ucType);

    if(xEvent.xEventFun) {

        xEvent.xEventFun(xEvent.xEventparam.pucData, xEvent.xEventparam.ulLen);
    }

    vPortFree(xEvent.xEventparam.pucData);

}


void APP_vEventSendBuff(uint8_t ucType, vKmlEventParam xFunc, uint8_t *pucData, uint32_t ulLen)
{
    
    if(ucType >= eEventMax) {

        return ;
    }

    if(ulLen <= 0x00 || NULL == pucData) {

        return ;
    }

    if(RingBufFull(&xEventQueue[ucType].xEventRing)) {

        LYY_LOGE(">>> The Ringbuf is FULL ");
		return ;
	}

    APP_EVENT_T xEvent;

    xEvent.xEventparam.pucData = pvPortMalloc(ulLen);

    if(NULL == xEvent.xEventparam.pucData) {

        LYY_LOGE(">>> pxEvent.xEventparam.pucData MALLOC FAIL");
        return;
    }

    memset(xEvent.xEventparam.pucData, 0x00, ulLen);

    memcpy(xEvent.xEventparam.pucData, pucData, ulLen);
    
    xEvent.xEventFun           = xFunc;
    xEvent.xEventparam.ulLen   = ulLen;

    RingBufWriteOne(&xEventQueue[ucType].xEventRing, (void *)&xEvent);

    LYY_LOGD(">>> xeventSend buf Success, index= [%d] ", ucType);

}




static void prvStartSendEventTask(void *pvArg)
{

    (void)pvArg;

#define SUB_TIME                (50U)

    while(1) {

        APP_prvEventRecv(eEventStart);
        vTaskDelay(pdMS_TO_TICKS(SUB_TIME));
    }

}



void APP_vEventTaskDetele(void)
{

    uint32_t    ulCnt   = 0x00;

    for( ; ulCnt < eEventMax ; ulCnt++) {

        if(xEventQueue[ulCnt].xEventTask) {

            vTaskDelete(xEventQueue[ulCnt].xEventTask);
        }
        
    }

}


void APP_vEventTaskCreate(void)
{

#define UPGRADE_EVENT_PRIO          (8U)

    xTaskCreate(    prvStartSendEventTask, 
                    "eEventStart", 
                    (1024 * 4) / sizeof(portSTACK_TYPE), 
                    NULL,
                    UPGRADE_EVENT_PRIO, 
                    &xEventQueue[eEventStart].xEventTask);

}

void APP_vEvenInit(void)
{

    uint32_t    ulCnt   = 0x00;

    for( ; ulCnt < eEventMax ; ulCnt++) {

        RingBufInit(&xEventQueue[ulCnt].xEventRing, 
                    xEventQueue[ulCnt].xEventBuf, 
                    sizeof(APP_EVENT_T), 
                    KML_EVENT_MAX_COUNT);
    }

}




