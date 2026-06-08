#include <stdio.h>


#include "HAL_Log.h"
#include "HAL_Gpio.h"
#include "HAL_System.h"

#include "APP_Detect.h"



typedef struct 
{
	uint16_t    usGatCnt;               /* 采集 */
    uint16_t    usTriggerCond;          /* 触发条件 */
    uint16_t    usGpio;                 /* 采集IO */
    uint32_t    ulTriggerNumer;         /* 触发次数 */
	void	    (*vFunc)(void *pvArg);
}APP_DETECT_CB_T;



#define APP_MODE_CRTL_OPEN                  HAL_vGpioSet(GPIO_COIN_CTRL,   GPIO_HIGH    )
#define APP_MODE_CRTL_CLOSE                 HAL_vGpioSet(GPIO_COIN_CTRL,   GPIO_LOW     )

#define APP_GET_ELE_SWITCH                  HAL_vGpioGet(GPIO_MODE_CTRL)

#define APP_COIN_OUT_OPEN                   HAL_vGpioSet(GPIO_COIN_OUT,   GPIO_HIGH    )
#define APP_COIN_OUT_CLOSE                  HAL_vGpioSet(GPIO_COIN_OUT,   GPIO_LOW     )


#define APP_DETECT_TASK_TIME                (50U)                                                           /* 50MS轮询一次任务 */
#define APP_NETWORK_DETECT_TIME             (3000U)                                                         /* 配网键检测3s */
#define APP_DETECT_PULSE_MARK_FLAG          (0xff00)                                                        /* ———————————__________ */


static APP_DETECT_CB_T g_xDetect[APP_DETECT_MAX] = {   {    0x00, 
                                                            (APP_NETWORK_DETECT_TIME/APP_DETECT_TASK_TIME),  
                                                            GPIO_NETWORK_TRIGGER,   
                                                            0x00, 
                                                            NULL}, 
                                                            
                                                        {   0x00, 
                                                            (APP_DETECT_PULSE_MARK_FLAG),                
                                                            GPIO_GIFT_OUT,          
                                                            0x00, 
                                                            NULL}, 

                                                        {   0x00, 
                                                            (APP_DETECT_PULSE_MARK_FLAG),                
                                                            GPIO_COIN_IN,           
                                                            0x00, 
                                                            NULL},     
                                                    };


static uint32_t g_ulMachtineLvl;


static void APP_prvDetectNetWorkKey(void)
{
    uint8_t ucLvl;

    ucLvl = HAL_vGpioGet(g_xDetect[APP_NETWORK_DETECT].usGpio);

    if(GPIO_LOW == ucLvl)  {

        g_xDetect[APP_NETWORK_DETECT].usGatCnt++;
    }
    else {

        g_xDetect[APP_NETWORK_DETECT].usGatCnt = 0x00;
    }

    if(g_xDetect[APP_NETWORK_DETECT].usTriggerCond == g_xDetect[APP_NETWORK_DETECT].usGatCnt)  {

        if(g_xDetect[APP_NETWORK_DETECT].vFunc)  {

            g_xDetect[APP_NETWORK_DETECT].vFunc(NULL);
        }

        g_xDetect[APP_NETWORK_DETECT].usGatCnt = 0x00;

    }

}


static void APP_prvDetectTask(void *pvArg)
{

    (void)pvArg;

    uint32_t    ucCnt;

#ifdef CFG_NO_OS

    for(ucCnt = 0x00; ucCnt < APP_DETECT_MAX; ucCnt++)  {

        switch(ucCnt)
        {
            case APP_NETWORK_DETECT : {

#ifndef CFG_TEST_LOG
                APP_prvDetectNetWorkKey();
#endif
            }break;

            /* 投币、退礼检测 */
            default :  {

                if(g_xDetect[ucCnt].ulTriggerNumer > 0) {

                    if(g_xDetect[ucCnt].vFunc){

                        g_xDetect[ucCnt].vFunc(&g_xDetect[ucCnt].ulTriggerNumer);
                    }

                    g_xDetect[ucCnt].ulTriggerNumer = 0x00;

                }

            };

        }
    }

#else

    for( ; ; )
    {

        for(ucCnt = 0x00; ucCnt < APP_DETECT_MAX; ucCnt++)
        {
            switch(ucCnt)
            {
                case APP_NETWORK_DETECT :
                {
#ifndef CFG_TEST_LOG
                    APP_prvDetectNetWorkKey();
#endif
                }break;

                /* 投币、退礼检测 */
                default : 
                {

                    if(g_xDetect[ucCnt].ulTriggerNumer > 0)
                    {

                        if(g_xDetect[ucCnt].vFunc)
                        {
                            g_xDetect[ucCnt].vFunc(&g_xDetect[ucCnt].ulTriggerNumer);
                        }

                        g_xDetect[ucCnt].ulTriggerNumer = 0x00;

                    }

                };

            }
        }

        vTaskDelay(APP_DETECT_TASK_TIME  / portTICK_PERIOD_MS);

    }

#endif

}


void APP_vDeteclRegisterFunc(uint16_t usDetectType, void (*vFunc)(void *pvArg))
{
    g_xDetect[usDetectType].vFunc = vFunc;
}



void APP_vDetecTimerMs(void)
{

    volatile uint16_t usCnt;

    for(usCnt = APP_INPUT_GIFT; usCnt < APP_DETECT_MAX; usCnt++) {

        g_xDetect[usCnt].usGatCnt <<=  0x01;
        g_xDetect[usCnt].usGatCnt  |=  HAL_vGpioGet(g_xDetect[usCnt].usGpio);

        if(g_xDetect[usCnt].usTriggerCond == g_xDetect[usCnt].usGatCnt) {

            g_xDetect[usCnt].ulTriggerNumer++;
        }

    }

}



void APP_prvDetectMachtineLvl(void)
{

    APP_MODE_CRTL_CLOSE;

    g_ulMachtineLvl = APP_GET_ELE_SWITCH;

    LYY_LOGD(">>> ele switch :[%d]", g_ulMachtineLvl);

    g_ulMachtineLvl ? APP_COIN_OUT_OPEN : APP_COIN_OUT_CLOSE;

    APP_MODE_CRTL_OPEN;

}



uint32_t APP_ulGetMachtineLvl(void)
{
    return g_ulMachtineLvl;
}



void APP_vDetectInit(void)
{

    APP_prvDetectMachtineLvl();

#ifdef CFG_NO_OS

    static struct HalSoftTimer AppDetTask;

    HAL_vSoftTimeInit(&AppDetTask, 
                      APP_prvDetectTask, 
                      APP_DETECT_TASK_TIME,
                      NULL);

#else

#define DETECT_THREAD_NAME          "DETECT_THREAD"
#define DETECT_THREAD_STACK         (1024 * 2 / sizeof(portSTACK_TYPE))

    xTaskCreate(    APP_prvDetectTask, 
                    DETECT_THREAD_NAME, 
                    DETECT_THREAD_STACK, 
                    NULL, 
                    5,
                    NULL);

#endif

}







