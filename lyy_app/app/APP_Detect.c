#include <stdio.h>


#include "HAL_Log.h"
#include "HAL_Gpio.h"
#include "HAL_System.h"

#include "APP_Detect.h"
#include "APP_Pluse.h"
#include "lyy_para.h"
#include "lyy_tjc.h"



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
                                                            GPIO_COIN_IN,           
                                                            0x00, 
                                                            NULL},     
                                                    };


static uint32_t g_ulMachtineLvl;
static void (*g_pvCoinLanes)(uint32_t lane1, uint32_t lane2);
static uint32_t g_aLaneCoins[2];
static uint8_t g_ucCoinPosition;


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


/** @brief 投币检测任务取走各货道的计数，再交给业务回调。 */
static void APP_prvDispatchCoin(void)
{
    if (g_pvCoinLanes) {
        uint32_t lane1 = __atomic_exchange_n(&g_aLaneCoins[0], 0U, __ATOMIC_RELAXED);
        uint32_t lane2 = __atomic_exchange_n(&g_aLaneCoins[1], 0U, __ATOMIC_RELAXED);
        if (lane1 || lane2) g_pvCoinLanes(lane1, lane2);
    } else {
        APP_DETECT_CB_T *coin = &g_xDetect[APP_INPUT_COIN];
        if (coin->ulTriggerNumer > 0) {
            if (coin->vFunc) coin->vFunc(&coin->ulTriggerNumer);
            coin->ulTriggerNumer = 0;
        }
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

            /* 投币检测 */
            case APP_INPUT_COIN: APP_prvDispatchCoin(); break;

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

                /* 投币检测 */
                case APP_INPUT_COIN: APP_prvDispatchCoin(); break;

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



/**
 * @brief 注册双货道投币回调，启用脉冲起点的货道归属采样。
 * @param callback 接收货道 1、2 的有效脉冲数；NULL 使用原单计数流程。
 * @pre 在检测任务和 1 ms 定时器启动前注册。
 */
void APP_vDetectRegisterCoinLanes(void (*callback)(uint32_t lane1, uint32_t lane2))
{
    g_pvCoinLanes = callback;
}

/**
 * @brief 每毫秒采样 COIN_IN，按原 8 高、8 低规则确认一个脉冲。
 * @details 双货道在首个低采样锁定屏幕货道，确认脉冲后才累加。
 * @note 本函数仅采样和累计，不写 Flash、不触发 MQTT。
 */
void APP_vDetecTimerMs(void)
{

    APP_DETECT_CB_T *pxDetect = &g_xDetect[APP_INPUT_COIN];

    pxDetect->usGatCnt <<= 1;
    pxDetect->usGatCnt |= HAL_vGpioGet(pxDetect->usGpio);
    if (g_pvCoinLanes) {
        /* Latch on the first low sample after eight high samples; only
         * book it when the existing eight-low debounce validates the pulse. */
        if ((pxDetect->usGatCnt & 0x1ffU) == 0x1feU) {
            g_ucCoinPosition = lyy_tjc_coin_position();
        }
        if (pxDetect->usTriggerCond == pxDetect->usGatCnt) {
            __atomic_fetch_add(&g_aLaneCoins[g_ucCoinPosition == 2U], 1U, __ATOMIC_RELAXED);
        }
    } else if (pxDetect->usTriggerCond == pxDetect->usGatCnt) {
        pxDetect->ulTriggerNumer++;
    }

}



/** @brief 采样 MODE_CTRL，并以检测出的空闲电平初始化双路输出。 */
void APP_prvDetectMachtineLvl(void)
{

    const bool bDualLane = (get_dev_type_code() == DEVICE_TYPE_PULSES);
    APP_MODE_CRTL_CLOSE;

    g_ulMachtineLvl = APP_GET_ELE_SWITCH;

    LYY_LOGD(">>> ele switch :[%d]", g_ulMachtineLvl);

    g_ulMachtineLvl ? APP_COIN_OUT_OPEN : APP_COIN_OUT_CLOSE;
    /* For the dual-lane type, both outputs use the sampled machine idle. */
    APP_vPulseOutputInit(bDualLane);

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







