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

static void APP_prvTimerMs(void *pvArg)
{

    (void)pvArg;

    APP_prvDetectNetWorkKey();


}


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







