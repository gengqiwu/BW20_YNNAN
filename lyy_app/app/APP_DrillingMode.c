/**
 * @file APP_DrillingMode.c
 * @brief 累加纸钞盒相关账目，周期保存累计值并在 MQTT 联网时提交内存增量。
 * @note 待报增量仅在内存中，不是持久离线队列；本模块不提供并发互斥。
 */
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

/**
 * @brief 累加入币待报增量及累计账目，标记下次周期保存。
 * @param[in] pvArg 指向有效 uint32_t 入币数量；调用期间读取，不接管所有权。
 * @note 必须先初始化账目指针；纸钞盒脉冲由 MCU 控制，此处不驱动本地 GPIO。
 */
void APP_vCoinIn(void *pvArg)
{
    g_DrillSta.ucAccUpdate = 1;
    g_DrillSta.xAcc.ulCoinInSum += *((uint32_t *)pvArg);
    g_DrillSta.pxAcc->ulCoinInSum += *((uint32_t *)pvArg);
    /* Paper-box pulses are controlled by the MCU, not by a local GPIO. */
    LYY_LOGD(">>> CoinIn sum : [%d][%d]", g_DrillSta.pxAcc->ulCoinInSum,
             *((uint32_t *)pvArg));
}

/**
 * @brief 累加出礼待报增量及累计账目，标记下次周期保存。
 * @param[in] pvArg 指向有效 uint32_t 出礼数量；须在账目初始化后调用。
 */
void APP_vGiftOut(void *pvArg)
{
    g_DrillSta.ucAccUpdate = 1;
    g_DrillSta.xAcc.ulGiftOutSum += *((uint32_t *)pvArg);
    g_DrillSta.pxAcc->ulGiftOutSum += *((uint32_t *)pvArg);
    LYY_LOGD(">>> GiftOut sum : [%d][%d]", g_DrillSta.pxAcc->ulGiftOutSum,
             *((uint32_t *)pvArg));
}

/**
 * @brief 标记娃娃机账目类型并累加入币数量。
 * @param[in] pvArg 指向有效 uint32_t 入币数量，传给通用入币处理。
 */
void APP_vWwjCoinIn(void *pvArg)
{
    g_DrillSta.ucAccWwjUpdate = 1;
    APP_vCoinIn(pvArg);
}

/**
 * @brief 标记娃娃机账目类型并累加出礼数量。
 * @param[in] pvArg 指向有效 uint32_t 出礼数量，传给通用出礼处理。
 */
void APP_vWwjGiftOut(void *pvArg)
{
    g_DrillSta.ucAccWwjUpdate = 1;
    APP_vGiftOut(pvArg);
}

/**
 * @brief 标记兑币机账目类型并累加入币数量。
 * @param[in] pvArg 指向有效 uint32_t 入币数量，传给通用入币处理。
 */
void APP_vDbjCoinIn(void *pvArg)
{
    g_DrillSta.ucAccDbjUpdate = 1;
    APP_vCoinIn(pvArg);
}

/**
 * @brief 累加兑币机出币增量及累计值，标记保存和兑币机上报。
 * @param[in] pvArg 指向有效 uint32_t 出币数量；须在账目初始化后调用。
 */
void APP_vDbjCoinOut(void *pvArg)
{
    g_DrillSta.ucAccUpdate = 1;
    g_DrillSta.ucAccDbjUpdate = 1;
    g_DrillSta.xAcc.ulCoinOutSum += *((uint32_t *)pvArg);
    g_DrillSta.pxAcc->ulCoinOutSum += *((uint32_t *)pvArg);
    LYY_LOGD(">>> CoinOut sum : [%d][%d]", g_DrillSta.pxAcc->ulCoinOutSum,
             *((uint32_t *)pvArg));
}

/**
 * @brief 有账目变动时调用参数保存接口，并清除更新标志。
 * @note 保存的是参数中的累计值，不含内存待报增量；未检查保存返回值，失败也清标志。
 */
void APP_vAccUpdate(void)
{
    if (g_DrillSta.ucAccUpdate) {
        APP_lSetParamUpdate();
        g_DrillSta.ucAccUpdate = 0;
    }
}

/**
 * @brief 按兑币机、娃娃机、通用脉冲的优先次序提交待报增量。
 * @note 调用方负责判断 MQTT 联网；类型分支共用增量，非独立设备账目队列。
 * @warning 调用事件接口后即清除对应增量，不等待送达确认；断电会丢失尚存内存的待报增量。
 */
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

/**
 * @brief 周期保存累计账目，仅在 MQTT 已连接时尝试补报内存增量。
 * @param[in] pvArg 软件定时器参数，未使用。
 * @note 离线时保留内存增量至后续联网周期；不提供跨重启的逐笔补报保证。
 */
void APP_vAccTask(void *pvArg)
{
    (void)pvArg;
    APP_vAccUpdate();
    if (lyy_mqtt_connect_result()) {
        APP_vReportedAcc();
    }
}

/**
 * @brief 绑定参数中的累计账目，并注册 4 秒周期的账目处理回调。
 * @note 应在任何入币、出币或出礼回调前完成初始化。
 */
void APP_vDrillingModeInit(void)
{
    static struct HalSoftTimer AppAccTask;
    g_DrillSta.pxAcc = APP_pxlGetParameAcc();
    HAL_vSoftTimeInit(&AppAccTask, APP_vAccTask, 4 * 1000, NULL);
}
