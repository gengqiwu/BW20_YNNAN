/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-11-22 15:29:42
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-10 15:02:36
 * @FilePath: \ameba-rtos\lyy_app\hal\HAL_System.c
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "HAL_Log.h"
#include "HAL_System.h"

#include "sys_api.h"
#include "timers.h"
#include "timer_api.h"
#include "os_wrapper.h"

#include "lwip/inet.h"
#include "wifi_intf_drv_to_app_cast.h"


#if defined (CONFIG_AMEBADPLUS)
#define WDG_RESET_SRC	(AON_BIT_RSTF_IWDG | AON_BIT_RSTF_WDG0 | AON_BIT_RSTF_WDG1 | AON_BIT_RSTF_WDG2)
#if defined (CONFIG_ARM_CORE_CM0)
    WDG_TypeDef     *WDG_DEV    = IWDG_DEV;
    IRQn_Type       WDG_IRQNUM  = IWDG_IRQ;
#elif defined (CONFIG_ARM_CORE_CM4)
    WDG_TypeDef     *WDG_DEV    = KM4_NS_WDG_DEV;
    IRQn_Type       WDG_IRQNUM  = KM4_NS_WDG_IRQ;
#endif
#endif


static struct HalSoftTimer *g_pxHandle = NULL; 


uint64_t HAL_ulGetTimeMs(void)
{
    return rtos_time_get_current_system_time_ms();
}

void HAL_vSoftTimeInit(struct HalSoftTimer *pxHandle, 
                       void(*vTimeoutCb)(void *), 
                       unsigned long ullRepeat,
                       void *pxArg)
{

    pxHandle->vTimeoutCb = vTimeoutCb;
    pxHandle->ullSysTick = rtos_time_get_current_system_time_ms();
    pxHandle->ullRepeat  = ullRepeat;
    pxHandle->pxArg      = pxArg;

    struct HalSoftTimer *pxTarget = g_pxHandle;
    
    while(pxTarget) {
        
        if(pxTarget == pxHandle) {
            
            return ;
        }
        
        pxTarget = pxTarget->pxNext;
    }
    
    /* APP创建关联即可 */
    pxHandle->pxNext    = g_pxHandle;
    g_pxHandle          = pxHandle;

}



void HAL_vSoftTimeLoop(void)
{

	uint32_t ulCurTick    = 0x00;
    uint32_t ulDiff       = 0x00;

    struct HalSoftTimer *pxTarget = g_pxHandle;

    ulCurTick = rtos_time_get_current_system_time_ms();
    
    for(; pxTarget ; pxTarget = pxTarget->pxNext) {
        
        if(ulCurTick < pxTarget->ullSysTick) {
            
            ulDiff = 0xffffffffUL - pxTarget->ullSysTick + ulCurTick;

        }else{

            ulDiff = ulCurTick  - pxTarget->ullSysTick;
        }

        if(ulDiff >= pxTarget->ullRepeat) {

            pxTarget->ullSysTick = rtos_time_get_current_system_time_ms();

            pxTarget->vTimeoutCb(pxTarget->pxArg);

       }
    }

}


uint32_t HAL_ulGetSysTick(void)
{
    return rtos_time_get_current_system_time_ms();
}


uint8_t HAL_ucTickTimeout(uint32_t ulSysTick, uint32_t ulTimeoutMs)
{
    
	uint32_t ulCurTick = 0x00;
    uint32_t ulDiff    = 0x00;
	
	ulCurTick = rtos_time_get_current_system_time_ms();

	if(ulCurTick < ulSysTick) {

		ulDiff = 0xffffffffUL - ulSysTick + ulCurTick;
	}else {

		ulDiff = ulCurTick - ulSysTick;
	}

	return ulDiff >= ulTimeoutMs ? 1 : 0;

}


static uint32_t prvulGtimerTimeoutHandler(void *pvArg)
{

	HAL_TIME_HANDLE_T *pxTimerHandle = (HAL_TIME_HANDLE_T *)pvArg;

	RTIM_INTClear(TIMx[pxTimerHandle->ucTimerId]);

    if(pxTimerHandle->prvTimerHandle) {
        
        pxTimerHandle->prvTimerHandle(pxTimerHandle->pvArg);
    }

	RTIM_INTClear(TIMx[pxTimerHandle->ucTimerId]);

	return 0;
}



void HAL_vRtosTimerCreate(  HAL_RTOS_TIMER_T *pxhandle, 
                            const char *pcTimerName, 
                            uint32_t ucTimerId,
                            uint32_t ulIntervalMs, 
                            uint8_t ucReload, 
                            void (*pvTimerCallback)(void *))
{

	TickType_t xTimerTicks;

	if (pxhandle == NULL || pvTimerCallback == NULL) {

		return ;
	}

	xTimerTicks = (TickType_t)((ulIntervalMs + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS);

	*pxhandle = xTimerCreate(   pcTimerName, 
                                xTimerTicks, 
                                (BaseType_t)ucReload,
                                (void *)ucTimerId, 
                                (TimerCallbackFunction_t)pvTimerCallback);

	if (*pxhandle == NULL) {

		return ;
	}

#define HAL_RTOS_CONVERT_MS_TO_TICKS(MS) \
        ((MS == 0xFFFFFFFFUL) ? 0xFFFFFFFFUL : (TickType_t)((MS + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS))

    xTimerStart((TimerHandle_t)(*pxhandle), HAL_RTOS_CONVERT_MS_TO_TICKS(ulIntervalMs));

}


void HAL_vTimeUsCreate(HAL_TIME_HANDLE_T *pxTimerHandle, uint32_t ulTimeUs, const char *pcName)
{
    
    (void)pcName;

    if(pxTimerHandle->ucTimerId > eTimer7) {

        LYY_LOGE("### timer id err!!!");
        return ;
    }


    RTIM_TimeBaseInitTypeDef xTIMInitStruct;

    RCC_PeriphClockCmd( APBPeriph_TIMx[pxTimerHandle->ucTimerId], 
                        APBPeriph_TIMx_CLOCK[pxTimerHandle->ucTimerId], 
                        ENABLE);

    RTIM_TimeBaseStructInit(&xTIMInitStruct);

	xTIMInitStruct.TIM_Idx           = pxTimerHandle->ucTimerId;
	xTIMInitStruct.TIM_Period        = (uint32_t)((float)ulTimeUs / 1000000.0 * 32768) - 1;
    xTIMInitStruct.TIM_UpdateEvent   = ENABLE;
    xTIMInitStruct.TIM_UpdateEvent   = TIM_UpdateSource_Overflow;
    xTIMInitStruct.TIM_UpdateEvent   = ENABLE;

    RTIM_TimeBaseInit(  TIMx[pxTimerHandle->ucTimerId], 
                        &xTIMInitStruct, 
                        TIMx_irq[pxTimerHandle->ucTimerId], 
                        (IRQ_FUN)prvulGtimerTimeoutHandler, 
                        (u32)pxTimerHandle);

	RTIM_INTConfig( TIMx[pxTimerHandle->ucTimerId], 
                    TIM_IT_Update, 
                    ENABLE);

	RTIM_Cmd(   TIMx[pxTimerHandle->ucTimerId], 
                ENABLE);

}


void HAL_vGetDevMac(char *pcDevMac)
{

    struct _rtw_mac_t pMac;

    wifi_get_mac_address(0, &pMac, 0);

    sprintf(pcDevMac, "%02x%02x%02x%02x%02x%02x",   pMac.octet[0], 
                                                    pMac.octet[1], 
                                                    pMac.octet[2], 
                                                    pMac.octet[3], 
                                                    pMac.octet[4], 
                                                    pMac.octet[5]);

}

void HAL_vIWatchdogRefresh(void)
{
	WDG_Refresh(WDG_DEV);
}


uint32_t HAL_vIWatchdogIrqHandler(void *pvArg)
{
    (void)pvArg;

    printf("### err HAL_vIWatchdogIrqHandler\r\n");

    WDG_Refresh(WDG_DEV);
	WDG_ClearINT(WDG_DEV, WDG_BIT_EIC);

    System_Reset();

    return 1;
}


void HAL_vIWatchdong(uint32_t ulTimeMs)
{

	WDG_InitTypeDef WDG_InitStruct;

	WDG_StructInit(&WDG_InitStruct);

	WDG_InitStruct.Timeout  = ulTimeMs;
	WDG_InitStruct.EICNT    = ulTimeMs > 100 ? 100 : ulTimeMs / 2;
	WDG_InitStruct.EIMOD    = ENABLE;

	WDG_Init(WDG_DEV, &WDG_InitStruct);

	WDG_ClearINT(WDG_DEV, WDG_BIT_EIC);

    InterruptRegister((IRQ_FUN)HAL_vIWatchdogIrqHandler, WDG_IRQNUM, (u32)NULL, INT_PRI_MIDDLE);
	InterruptEn(WDG_IRQNUM, INT_PRI_MIDDLE);

	WDG_INTConfig(WDG_DEV, WDG_BIT_EIE, ENABLE);

    WDG_Enable(WDG_DEV);

}

uint32_t HAL_ulGetRandom(void)
{

    uint32_t ulRand;

    TRNG_get_random_bytes((uint8_t *)&ulRand, sizeof(uint32_t));

    return ulRand;

}


void HAL_vReset(char *pucReason, uint32_t ulDelayMs)
{

    LYY_LOGI(">>> reset .... [%s]", pucReason);

    vTaskDelay(ulDelayMs / portTICK_PERIOD_MS);

    System_Reset();

}


