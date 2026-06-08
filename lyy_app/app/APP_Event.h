/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-06-21 14:55:32
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-08-08 21:43:18
 * @FilePath: \lyy_std_esp32c2\main\app\APP_event.h
 */
#ifndef __APP_EVENT_H__
#define __APP_EVENT_H__

#include "ringbuf.h"
#include "HAL_System.h"

#define KML_EVENT_MAX_COUNT	    (30U)


/* 资源不够做不了太多队列 */
enum {
	
	eEventStart,					/* 启动支付 */
	eEventMax

};


typedef void (*vKmlEventParam)(uint8_t *pucData, uint32_t ulLen);

#pragma pack(push, 1)


typedef struct {

	uint32_t  ulLen;
    uint8_t   *pucData;

}APP_EVENT_PARAM_T;

typedef struct
{
	uint8_t            type;
	APP_EVENT_PARAM_T  xEventparam;
	vKmlEventParam     xEventFun;

}APP_EVENT_T;


typedef struct
{
	APP_EVENT_T     	xEventBuf[KML_EVENT_MAX_COUNT];
	tRingBufObject      xEventRing;
	TaskHandle_t 		xEventTask;
}APP_EVENT_RINGBUF_T;


#pragma pack(pop)


extern void APP_vEvenInit(void);
extern void APP_vEventTaskCreate(void);
extern void APP_vEventTaskDetele(void);
extern void APP_vEventSendBuff(uint8_t ucType, vKmlEventParam xFunc, uint8_t *pucData, uint32_t ulLen);


#endif

