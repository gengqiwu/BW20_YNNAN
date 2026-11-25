#include <stdio.h>
#include <stdint.h>

#include "HAL_Log.h"
#include "HAL_Gpio.h"
#include "HAL_System.h"

#include "APP_Param.h"
#include "APP_Pluse.h"
#include "APP_Detect.h"
#include <stdbool.h>

enum {

    ePluseOpen,
    ePluseClose,
};


typedef struct {

    uint32_t    usPulseCnt;             /* 启动次数 */
	uint32_t    usTimerCnt;             /* 记时 */
    uint32_t    usTriggerCond;          /* 触发时间条件 */
    uint32_t    ulLogo;                 /* 触发标志 */
	void	    (*vFunc)(void);
}APP_PULSE_STA_T;

enum {
    eCoinLane,
    eGiftLane,
    ePulseLanes,
};

typedef struct {
    uint32_t ulPending;      /* Includes the active pulse, if any. */
    uint32_t ulRemainingMs;  /* Active pulse or mandatory idle gap. */
    uint32_t ulWidthMs;
    uint32_t ulGapMs;
    bool bActive;
} APP_LANE_PULSE_T;

static APP_LANE_PULSE_T g_xLanePulse[ePulseLanes];
static uint32_t g_ulDeferredLegacy[APP_PULSE_MAX];
static uint32_t g_ulLegacyIdleMs = UINT32_MAX;
static bool g_bLegacyActive;
static bool g_bOutputsReady;
static uint8_t g_ucGiftIdle;
static bool g_bDualLaneReady;


static void APP_prvPulseStartOp(void);
static void APP_prvLevelStartOp(void);


#define APP_COIN_OUT_OPEN  APP_prvSetLegacyOutput(true)
#define APP_COIN_OUT_CLOSE APP_prvSetLegacyOutput(false)

static void APP_prvSetLegacyOutput(bool bActive)
{
    g_bLegacyActive = bActive;
    HAL_vGpioSet(GPIO_COIN_OUT, bActive ? !APP_ulGetMachtineLvl() : APP_ulGetMachtineLvl());
}


static APP_PULSE_STA_T g_xPulseInfo[] =     {   {   0x00, 
                                                    0x00, 
                                                    0x00, 
                                                    0x00,
                                                    APP_prvPulseStartOp},

                                                {   0x00, 
                                                    0x00, 
                                                    0x00, 
                                                    0x00,
                                                    APP_prvLevelStartOp}, 
                                            };



static void APP_prvLegacyStart(uint32_t ulPulseType, uint32_t ulPulseCnt)
{

    switch(ulPulseType)
    {
        case APP_COIN_PLUSE : {

            if( 0x00 == g_xPulseInfo[APP_COIN_PLUSE].usTimerCnt && 
                0x00 == g_xPulseInfo[APP_COIN_PLUSE].usTriggerCond) {

                APP_PULSE_T *pxPulse = APP_pxlGetParamePulse();

                g_xPulseInfo[APP_COIN_PLUSE].usTriggerCond  = pxPulse->ulPulseWidth; 
                g_xPulseInfo[APP_COIN_PLUSE].ulLogo         = ePluseOpen;

                APP_COIN_OUT_OPEN;
            }

            g_xPulseInfo[APP_COIN_PLUSE].usPulseCnt += ulPulseCnt;   

        }break;

        case APP_COIN_LEVEL : {

#define APP_LVL_TIME_MIN       (60U * 1000U)

            g_xPulseInfo[APP_COIN_LEVEL].usTriggerCond += ulPulseCnt * APP_LVL_TIME_MIN; 

            APP_COIN_OUT_OPEN;

        }break;

        default : break;

    }

}

void APP_vPulseStart(uint32_t ulPulseType, uint32_t ulPulseCnt)
{
    if (ulPulseType >= APP_PULSE_MAX) {
        return;
    }

    taskENTER_CRITICAL();
    if (g_xLanePulse[eCoinLane].ulRemainingMs != 0 ||
        g_ulDeferredLegacy[APP_COIN_PLUSE] != 0 || g_ulDeferredLegacy[APP_COIN_LEVEL] != 0) {
        /* COIN_IN retains its selected legacy mode. It must not cut short a
         * paid PA_8 pulse or its gap; resume it before the next paid pulse. */
        g_ulDeferredLegacy[ulPulseType] += ulPulseCnt;
    } else {
        APP_prvLegacyStart(ulPulseType, ulPulseCnt);
    }
    taskEXIT_CRITICAL();
}


static void APP_prvLevelStartOp(void)
{

    g_xPulseInfo[APP_COIN_LEVEL].usTriggerCond = 0; 

    APP_COIN_OUT_CLOSE;

}


static void APP_prvPulseStartOp(void)
{

    APP_PULSE_T *pxPulse = APP_pxlGetParamePulse();

    if(ePluseOpen == g_xPulseInfo[APP_COIN_PLUSE].ulLogo) {

        g_xPulseInfo[APP_COIN_PLUSE].usTriggerCond = 
                        0x00 == --g_xPulseInfo[APP_COIN_PLUSE].usPulseCnt ? 0x00 : pxPulse->ulPulseSpace;

        g_xPulseInfo[APP_COIN_PLUSE].ulLogo         = ePluseClose;

        APP_COIN_OUT_CLOSE;

    }
    else {

        g_xPulseInfo[APP_COIN_PLUSE].usTriggerCond  = pxPulse->ulPulseWidth;
        g_xPulseInfo[APP_COIN_PLUSE].ulLogo         = ePluseOpen;

        APP_COIN_OUT_OPEN;
        
    }

}


void APP_vPulseOutputInit(uint8_t bDualLane)
{
    g_ucGiftIdle = bDualLane ? APP_ulGetMachtineLvl() : GPIO_LOW;
    HAL_vGpioEnableOutput(GPIO_GIFT_OUT, g_ucGiftIdle);
    g_bDualLaneReady = bDualLane;
    g_bOutputsReady = true;
}

static bool APP_prvTryLanePulseStart(uint16_t usLane, uint32_t ulPulseCnt)
{
    if (usLane >= ePulseLanes || ulPulseCnt == 0) {
        return false;
    }

    bool bAccepted = false;
    taskENTER_CRITICAL();
    APP_LANE_PULSE_T *pxLane = &g_xLanePulse[usLane];
    if (g_bOutputsReady && ulPulseCnt <= UINT32_MAX - pxLane->ulPending) {
        if (pxLane->ulPending == 0 && pxLane->ulRemainingMs == 0) {
            const APP_PULSE_T *pxPulse = APP_pxlGetParamePulse();
            pxLane->ulWidthMs = pxPulse->ulPulseWidth;
            /* An idle tick is required even when configured gap is zero. */
            pxLane->ulGapMs = pxPulse->ulPulseSpace ? pxPulse->ulPulseSpace : 1U;
        }
        if (pxLane->ulWidthMs != 0) {
            pxLane->ulPending += ulPulseCnt;
            bAccepted = true;
        }
    }
    taskEXIT_CRITICAL();
    return bAccepted;
}

uint8_t APP_bTryLanePulseStart(uint16_t usLane, uint32_t ulPulseCnt)
{
    return g_bDualLaneReady && APP_prvTryLanePulseStart(usLane, ulPulseCnt);
}

void APP_vGiftPulseStart(uint32_t ulPulseCnt)
{
    (void)APP_prvTryLanePulseStart(eGiftLane, ulPulseCnt);
}

static bool APP_prvLegacyBusy(void)
{
    return g_bLegacyActive || g_xPulseInfo[APP_COIN_PLUSE].usTriggerCond != 0 ||
           g_xPulseInfo[APP_COIN_LEVEL].usTriggerCond != 0;
}

static void APP_prvLanePulseTimerMs(uint16_t usLane, uint32_t ulGpio, uint8_t ucIdle)
{
    APP_LANE_PULSE_T *pxLane = &g_xLanePulse[usLane];
    if (pxLane->ulRemainingMs != 0) {
        if (--pxLane->ulRemainingMs != 0) {
            return;
        }
        if (pxLane->bActive) {
            HAL_vGpioSet(ulGpio, ucIdle);
            pxLane->bActive = false;
            --pxLane->ulPending;
            pxLane->ulRemainingMs = pxLane->ulGapMs;
            return;
        }
    }

    if (usLane == eCoinLane) {
        /* Never drive PA_8 while legacy pulse/level owns it. Deferred legacy
         * requests take priority at a paid pulse boundary, not mid-pulse. */
        if (APP_prvLegacyBusy()) {
            return;
        }
        for (uint32_t ulType = 0; ulType < APP_PULSE_MAX; ++ulType) {
            if (g_ulDeferredLegacy[ulType] != 0) {
                uint32_t ulCount = g_ulDeferredLegacy[ulType];
                g_ulDeferredLegacy[ulType] = 0;
                APP_prvLegacyStart(ulType, ulCount);
                return;
            }
        }
        if (g_ulLegacyIdleMs < pxLane->ulGapMs) {
            return;
        }
    }

    if (pxLane->ulPending != 0) {
        HAL_vGpioSet(ulGpio, !ucIdle);
        pxLane->bActive = true;
        pxLane->ulRemainingMs = pxLane->ulWidthMs;
    }
}


void APP_vPulseimerMs(void)
{ 

    uint8_t ucCnt = 0x00;
#ifdef CFG_RTOS_TIME
    taskENTER_CRITICAL();
#endif
    bool bLegacyWasBusy = APP_prvLegacyBusy();

    for( ; ucCnt < APP_PULSE_MAX; ucCnt++) {

        if( g_xPulseInfo[ucCnt].usTriggerCond != 0x00 && 
            ++g_xPulseInfo[ucCnt].usTimerCnt == g_xPulseInfo[ucCnt].usTriggerCond) {

            if(g_xPulseInfo[ucCnt].vFunc)  {

                g_xPulseInfo[ucCnt].vFunc();
            }

            g_xPulseInfo[ucCnt].usTimerCnt = 0x00;

        }
    }
    if (bLegacyWasBusy) {
        g_ulLegacyIdleMs = 0;
    } else if (g_ulLegacyIdleMs != UINT32_MAX) {
        ++g_ulLegacyIdleMs;
    }
    APP_prvLanePulseTimerMs(eCoinLane, GPIO_COIN_OUT, APP_ulGetMachtineLvl());
    APP_prvLanePulseTimerMs(eGiftLane, GPIO_GIFT_OUT, g_ucGiftIdle);
#ifdef CFG_RTOS_TIME
    taskEXIT_CRITICAL();
#endif
}








