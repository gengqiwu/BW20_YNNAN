#include <stdio.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "task.h"
#include "HAL_Log.h"
#include "HAL_System.h"

#include "APP_Param.h"
#include "APP_DrillingMode.h"
#include "thing_mqtt.h"
#include "thing_event.h"

typedef struct {
    uint8_t ucAccUpdate;
    uint8_t ucAccDbjUpdate;
    uint8_t ucAccWwjUpdate;
    APP_ACC_T *pxAcc;
    APP_ACC_T xAcc;
} APP_DRILL_STA_T;

static APP_DRILL_STA_T g_DrillSta;

void APP_vCoinIn(void *pvArg)
{
    g_DrillSta.ucAccUpdate = 1;
    g_DrillSta.xAcc.ulCoinInSum += *((uint32_t *)pvArg);
    g_DrillSta.pxAcc->ulCoinInSum += *((uint32_t *)pvArg);
    /* Paper-box pulses are controlled by the MCU, not by a local GPIO. */
    LYY_LOGD(">>> CoinIn sum : [%d][%d]", g_DrillSta.pxAcc->ulCoinInSum,
             *((uint32_t *)pvArg));
}

void APP_vGiftOut(void *pvArg)
{
    g_DrillSta.ucAccUpdate = 1;
    g_DrillSta.xAcc.ulGiftOutSum += *((uint32_t *)pvArg);
    g_DrillSta.pxAcc->ulGiftOutSum += *((uint32_t *)pvArg);
    LYY_LOGD(">>> GiftOut sum : [%d][%d]", g_DrillSta.pxAcc->ulGiftOutSum,
             *((uint32_t *)pvArg));
}

void APP_vWwjCoinIn(void *pvArg)
{
    g_DrillSta.ucAccWwjUpdate = 1;
    APP_vCoinIn(pvArg);
}

void APP_vWwjGiftOut(void *pvArg)
{
    g_DrillSta.ucAccWwjUpdate = 1;
    APP_vGiftOut(pvArg);
}

void APP_vDbjCoinIn(void *pvArg)
{
    g_DrillSta.ucAccDbjUpdate = 1;
    APP_vCoinIn(pvArg);
}

void APP_vDbjCoinOut(void *pvArg)
{
    g_DrillSta.ucAccUpdate = 1;
    g_DrillSta.ucAccDbjUpdate = 1;
    g_DrillSta.xAcc.ulCoinOutSum += *((uint32_t *)pvArg);
    g_DrillSta.pxAcc->ulCoinOutSum += *((uint32_t *)pvArg);
    LYY_LOGD(">>> CoinOut sum : [%d][%d]", g_DrillSta.pxAcc->ulCoinOutSum,
             *((uint32_t *)pvArg));
}

void APP_vAccUpdate(void)
{
    if (g_DrillSta.ucAccUpdate) {
        APP_lSetParamUpdate();
        g_DrillSta.ucAccUpdate = 0;
    }
}

void APP_vReportedAcc(void)
{
    if (g_DrillSta.ucAccDbjUpdate) {
        if (g_DrillSta.xAcc.ulCoinInSum || g_DrillSta.xAcc.ulCoinOutSum) {
            event_post_account_dbj_uart("offline_pay",
                g_DrillSta.pxAcc->ulCoinInSum, g_DrillSta.xAcc.ulCoinInSum,
                g_DrillSta.pxAcc->ulCoinOutSum, g_DrillSta.xAcc.ulCoinOutSum, NULL);
        }
        g_DrillSta.xAcc.ulCoinInSum = 0;
        g_DrillSta.xAcc.ulCoinOutSum = 0;
        g_DrillSta.ucAccDbjUpdate = 0;
        return;
    }

    if (g_DrillSta.ucAccWwjUpdate) {
        if (g_DrillSta.xAcc.ulCoinInSum || g_DrillSta.xAcc.ulGiftOutSum) {
            event_post_account_wwj_uart("offline_pay",
                g_DrillSta.pxAcc->ulCoinInSum, g_DrillSta.xAcc.ulCoinInSum,
                g_DrillSta.pxAcc->ulGiftOutSum, g_DrillSta.xAcc.ulGiftOutSum, NULL);
        }
        g_DrillSta.xAcc.ulCoinInSum = 0;
        g_DrillSta.xAcc.ulGiftOutSum = 0;
        g_DrillSta.ucAccWwjUpdate = 0;
        return;
    }

    if (g_DrillSta.xAcc.ulCoinInSum) {
        event_post_account_pulse(g_DrillSta.xAcc.ulCoinInSum,
            g_DrillSta.pxAcc->ulCoinInSum, "offline_pay", NULL, "accounts_pulse_coin");
        g_DrillSta.xAcc.ulCoinInSum = 0;
    }
    if (g_DrillSta.xAcc.ulGiftOutSum) {
        event_post_account_pulse(g_DrillSta.xAcc.ulGiftOutSum,
            g_DrillSta.pxAcc->ulGiftOutSum, "offline_pay", NULL, "accounts_pulse_gift");
        g_DrillSta.xAcc.ulGiftOutSum = 0;
    }
}

void APP_vAccTask(void *pvArg)
{
    (void)pvArg;
    APP_vAccUpdate();
    if (lyy_mqtt_connect_result()) {
        APP_vReportedAcc();
    }
}

void APP_vDrillingModeInit(void)
{
    static struct HalSoftTimer AppAccTask;
    g_DrillSta.pxAcc = APP_pxlGetParameAcc();
    HAL_vSoftTimeInit(&AppAccTask, APP_vAccTask, 4 * 1000, NULL);
}
