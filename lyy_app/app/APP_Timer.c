/**
 * @file APP_Timer.c
 * @brief 创建毫秒级周期回调，仅轮询配网按键，不采集现金脉冲。
 */
/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-06-19 14:35:04
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 11:14:30
 * @FilePath: \lyy_std_esp32c2\main\app\APP_Timer.c
 */
#include <stdio.h>


#include "HAL_Log.h"
#include "HAL_System.h"
#include "APP_Detect.h"


static void APP_prvTimerMs(void *pvArg);


#ifdef CFG_RTOS_TIME  

static HAL_RTOS_TIMER_T g_AppRtosTime;

#else

static HAL_TIME_HANDLE_T g_pxTimerHandle = {

    .ucTimerId      = eTimer1,
    .prvTimerHandle = APP_prvTimerMs,
    .pvArg          = NULL,
};

#endif

/**
 * @brief 周期调用配网按键检测，不执行本地投币或现金脉冲采样。
 * @param[in] pvArg 定时器参数，未使用。
 * @note 回调运行上下文由所选 HAL 定时器实现决定，不在此承诺中断或任务上下文。
 */
static void APP_prvTimerMs(void *pvArg)
{

    (void)pvArg;

    APP_prvDetectNetWorkKey();


}


/**
 * @brief 按配置创建自动周期 RTOS 定时器或 1000 微秒硬件定时器，驱动配网按键检测。
 * @note RTOS 分支传入周期值 1，实际时间单位由 HAL 定义；本模块不再注册现金脉冲采集。
 */
void APP_Timer1msInit(void)
{

#define APP_TIME_US           (1000U)

#ifdef CFG_RTOS_TIME 

    HAL_vRtosTimerCreate(   &g_AppRtosTime,
                            "Rtostimer",
                            0,
                            APP_TIME_US / APP_TIME_US,
                            1,
                            APP_prvTimerMs);
    
#else
    HAL_vTimeUsCreate(  &g_pxTimerHandle, 
                        APP_TIME_US,
                        "MS_TIMER");
#endif

}







