/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-11-22 15:29:44
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 14:25:12
 * @FilePath: \ameba-rtos\lyy_app\hal\HAL_System.h
 */
#ifndef __HAL_SYSTEM_H__
#define __HAL_SYSTEM_H__


#include "device.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"


typedef struct HalSoftTimer{
    
    unsigned long ullSysTick;
    unsigned long ullRepeat;
    void (*vTimeoutCb)(void *);
    struct HalSoftTimer* pxNext;
    void *pxArg;

}HalSoftTimer_t;



enum {

	eTimer0,
	eTimer1,
	eTimer2,
	eTimer3,
	eTimer4,
	eTimer5,
	eTimer6,
	eTimer7,
	eTimer8,
	eTimer9,
	eTimer10,
	eTimer11,

	eTimerMax,
};


typedef struct {

    uint8_t ucTimerId;
    void    *pvArg;
    void    (*prvTimerHandle)(void *pvArg);
} HAL_TIME_HANDLE_T;


typedef void * HAL_RTOS_TIMER_T;

extern void HAL_vRtosTimerCreate(   HAL_RTOS_TIMER_T *pxhandle, 
                                    const char *pcTimerName, 
                                    uint32_t ucTimerId,
                                    uint32_t ulIntervalMs, 
                                    uint8_t ucReload, 
                                    void (*pvTimerCallback)(void *));

extern void HAL_vTimeUsCreate(  HAL_TIME_HANDLE_T   *pxTimerHandle, 
                                uint32_t            ulTimeUs,
                                const char          *pcName);


extern void HAL_vGetDevMac(char *pcDevMac);
extern void HAL_vReset(char *pucReason, uint32_t ulDelayMs);


extern void HAL_vSoftTimeLoop(void);
extern void HAL_vSoftTimeInit(  struct HalSoftTimer *pxHandle, 
                                void(*vTimeoutCb)(void *), 
                                unsigned long ullRepeat,
                                void *pxArg);

extern uint32_t HAL_ulGetRandom(void);
extern uint64_t HAL_ulGetTimeMs(void);
extern uint32_t HAL_ulGetSysTick(void);
extern uint8_t  HAL_ucTickTimeout(uint32_t ulSysTick, uint32_t ulTimeoutMs);

/* watchdong */
void     HAL_vIWatchdogRefresh(void);
void     HAL_vIWatchdong(uint32_t ulTimeMs);

#endif


