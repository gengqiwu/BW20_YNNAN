/** @file APP_Led.c @brief 本地双色灯闪烁调度及纸钞盒系统状态缓存。 */
#include <stdio.h>

#include "USER_Config.h"

#include "HAL_Gpio.h"
#include "HAL_System.h"

#include "APP_Led.h"

static uint8_t led_status = LED_STATUS_START_SYSTEM;
/** @brief 返回供 STM32 的 0B 轮询使用的状态码。 @return LED_STATUS_* 缓存值。 */
uint8_t get_led_status(void) { return led_status; }
/** @brief 仅更新系统状态缓存。 @param[in] status STM32 网络状态协议编号。 */
void set_led_status(uint8_t status) { led_status = status; }

/* 慢闪间隔 450ms */
#define APP_LED_SLOW_FLASH                 (500U)
/* 快闪间隔 150ms */
#define APP_LED_QUICK_FLASH                (100U)


/* 绿灯控制 */
#define APP_LED_GREEN_OPEN                 HAL_vGpioSet(GPIO_LED_GREEN, GPIO_HIGH   )
#define APP_LED_GREEN_CLOSE                HAL_vGpioSet(GPIO_LED_GREEN, GPIO_LOW    )

/* 红灯控制 */
#define APP_LED_RED_OPEN                   HAL_vGpioSet(GPIO_LED_RED,   GPIO_HIGH   )
#define APP_LED_RED_CLOSE                  HAL_vGpioSet(GPIO_LED_RED,   GPIO_LOW    )


typedef void  (*vLedCtr)(uint8_t);


enum
{
    LED_OFF,
    LED_ON
};


typedef struct 
{
    uint8_t     ucLedSta;
	uint16_t    usOnTime;
	uint16_t    usOffTime;
	uint16_t	usAddTime;
    vLedCtr     xLedCtrCb;
}APP_LED_INFO_T;



typedef struct 
{
    uint8_t        ucLedSetSta;
    APP_LED_INFO_T g_xledInfo;
}APP_LED_FLASH_T;


static void APP_prvLedCtr(uint8_t ucLedType);


static APP_LED_FLASH_T g_xledFlash[LED_STA_MAX] = {

/* 灭 */
#define LED_CLOSE        {  LED_OFF, 0x00, 0x00, 0x00, NULL}

/* 常亮 */
#define LED_OPEN         {  LED_OFF, 0xff, 0xff, 0x00, NULL}

/* 慢闪 */
#define LED_SLOW_FLASH   {  LED_OFF,                                    \
                            APP_LED_SLOW_FLASH/APP_LED_QUICK_FLASH,     \
                            APP_LED_SLOW_FLASH/APP_LED_QUICK_FLASH,     \
                            0x00,                                       \
                            APP_prvLedCtr}                                      
/* 快闪 */
#define LED_QUICK_FLASH  {  LED_OFF,                                    \
                            APP_LED_QUICK_FLASH/APP_LED_QUICK_FLASH,    \
                            APP_LED_QUICK_FLASH/APP_LED_QUICK_FLASH,    \
                            0x00,                                       \
                            APP_prvLedCtr}

    {LED_STA_OFF            , LED_CLOSE           },
    {LED_STA_ON             , LED_OPEN            },
    {LED_STA_SLOW_FLASH     , LED_SLOW_FLASH      },
    {LED_STA_QUICK_FLASH    , LED_QUICK_FLASH     },

};


/* 默认为连网模式 */
static APP_LED_INFO_T g_xledInfo[LED_MAX] = {

    /* LED_GREEN */
    LED_CLOSE, 

    /* LED_RED */
    LED_SLOW_FLASH

};


void APP_vLedSstSta(uint8_t ucLedType, uint8_t ucLedSta)
{
    
    switch(ucLedSta)
    {
        case LED_STA_OFF :
        {
            LED_GREEN == ucLedType ? APP_LED_GREEN_CLOSE : APP_LED_RED_CLOSE;
        } break;

        case LED_STA_ON  :
        {
            LED_GREEN == ucLedType ? APP_LED_GREEN_OPEN : APP_LED_RED_OPEN;
        } break;

        default : ;
    }


    g_xledInfo[ucLedType] = g_xledFlash[ucLedSta].g_xledInfo;
    
}


static void APP_prvLedOpen(void)
{

#define APP_FALSH_CNT                       (3U)
#define APP_LED_OPEN                        do{APP_LED_GREEN_OPEN;  APP_LED_RED_OPEN ;}while(0)
#define APP_LED_CLOSE                       do{APP_LED_GREEN_CLOSE; APP_LED_RED_CLOSE;}while(0)


#ifdef CFG_NO_OS

    static uint8_t ucCnt = 0;

    if(ucCnt >= APP_FALSH_CNT * 2) {

        return ;
    }

    if(!(ucCnt++ % 2)) {

        APP_LED_OPEN;

    }else{

        APP_LED_CLOSE;
    }

    if(APP_FALSH_CNT * 2 == ucCnt) {
        
        uint32_t ulCnt = 0x00;

        for( ; ulCnt < LED_MAX; ulCnt++) {

            if(0xff == g_xledInfo[ulCnt].usOnTime) {

                switch(ulCnt)
                {
                    case  LED_GREEN : {

                        APP_LED_GREEN_OPEN;
                    }break;

                    case  LED_RED   : {

                        APP_LED_RED_OPEN;
                    }break;

                    default : ;
                }
            }
        }
    } 

#else

    uint8_t ucCnt = 0;

    for( ; ucCnt < APP_FALSH_CNT; ucCnt++) {

        APP_LED_OPEN;
        vTaskDelay(APP_LED_QUICK_FLASH / portTICK_PERIOD_MS);

        APP_LED_CLOSE;
        vTaskDelay(APP_LED_QUICK_FLASH / portTICK_PERIOD_MS);

    }

#endif
    
}


static void APP_prvLedCtr(uint8_t ucLedType)
{

    if(0x00 == g_xledInfo[ucLedType].usAddTime) {

        LED_OFF == g_xledInfo[ucLedType].ucLedSta ? 
                            LED_GREEN == ucLedType ? APP_LED_GREEN_OPEN  : APP_LED_RED_OPEN :
                            LED_GREEN == ucLedType ? APP_LED_GREEN_CLOSE : APP_LED_RED_CLOSE;

        g_xledInfo[ucLedType].ucLedSta = !g_xledInfo[ucLedType].ucLedSta;
    }

    uint32_t ulTime;

    ulTime = LED_OFF == g_xledInfo[ucLedType].ucLedSta ? g_xledInfo[ucLedType].usOffTime : g_xledInfo[ucLedType].usOnTime;

    g_xledInfo[ucLedType].usAddTime = g_xledInfo[ucLedType].usAddTime < ulTime ? (g_xledInfo[ucLedType].usAddTime + 1) : 0x00;

}



static void APP_PrvLedTask(void *pvArg)
{

    (void)pvArg;

    APP_prvLedOpen();

    uint32_t ulCnt;

#ifdef CFG_NO_OS

    for(ulCnt = 0x00; ulCnt < LED_MAX; ulCnt++) {

        if(g_xledInfo[ulCnt].xLedCtrCb) {

            g_xledInfo[ulCnt].xLedCtrCb(ulCnt);
        }

    }

#else

    for( ; ; ) {

        for(ulCnt = 0x00; ulCnt < LED_MAX; ulCnt++) {

            if(g_xledInfo[ulCnt].xLedCtrCb) {

                g_xledInfo[ulCnt].xLedCtrCb(ulCnt);
            }

        }  

        vTaskDelay(APP_LED_QUICK_FLASH / portTICK_PERIOD_MS);

    }

#endif

}



/**
 * @description: 初始化led
 * 
 * @return {*}
 */
void APP_vLedInit(void)
{


#ifdef CFG_NO_OS

    static struct HalSoftTimer AppLedTask;

    HAL_vSoftTimeInit(&AppLedTask, 
                      APP_PrvLedTask, 
                      APP_LED_QUICK_FLASH,
                      NULL);

#else

#define LED_THREAD_NAME          "LED_THREAD"
#define LED_THREAD_STACK         (512 / sizeof(portSTACK_TYPE))

    xTaskCreate(    APP_PrvLedTask, 
                    LED_THREAD_NAME, 
                    LED_THREAD_STACK, 
                    NULL, 
                    5,
                    NULL);

#endif

}


