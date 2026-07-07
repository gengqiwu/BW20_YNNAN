#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "ameba_soc.h"

#include "HAL_Gpio.h"

#include "gpio_irq_api.h" // mbed
#include "gpio_api.h"

typedef struct {

    uint32_t        ulGpioPin;
    uint32_t        ulGpioMode;
    uint32_t        ulGpioPupd;
}GPIO_INFO_T;



static const GPIO_INFO_T g_pxGpioInfo[] = {


#define GPIO_INIT_OUT(pin, Pupd)       {    pin,                             \
                                            GPIO_Mode_OUT,                   \
                                            Pupd}


#define GPIO_INIT_IN(pin, Pupd)         {   pin,                             \
                                            GPIO_Mode_IN,                    \
                                            Pupd}


/* 指示灯 */
#define    LED_RED                 (_PB_18)    
#define    LED_GREEN               (_PB_17)

    GPIO_INIT_OUT(LED_RED,          GPIO_PuPd_NOPULL),
    GPIO_INIT_OUT(LED_GREEN,        GPIO_PuPd_NOPULL),

 /* 配网按键 */
 #define    NETWORK_TRIGGER          (_PA_27)   

 GPIO_INIT_IN(NETWORK_TRIGGER,   GPIO_PuPd_UP),

/* 常开、常闭检测控制 */
#define     COIN_CTR                (_PB_19)
#define     MODE_CTRL               (_PA_12)

    GPIO_INIT_OUT(COIN_CTR,         GPIO_PuPd_NOPULL),
    GPIO_INIT_IN(MODE_CTRL,         GPIO_PuPd_NOPULL),

/* 投币、退礼检测 */
#define     GIFT_OUT                (_PB_21)
#define     COIN_IN                 (_PB_20)

    GPIO_INIT_IN(GIFT_OUT,          GPIO_PuPd_NOPULL),
    GPIO_INIT_IN(COIN_IN,           GPIO_PuPd_NOPULL),

/* 机台接口 */
#define     COIN_OUT                (_PA_8)

    GPIO_INIT_OUT(COIN_OUT,         GPIO_PuPd_NOPULL),

};


void HAL_vGpioSet(uint32_t ulIoType, uint8_t ucLevel)
{
    GPIO_WriteBit(g_pxGpioInfo[ulIoType].ulGpioPin, ucLevel);
}


uint8_t HAL_vGpioGet(uint32_t ulIoType)
{
    return GPIO_ReadDataBit(g_pxGpioInfo[ulIoType].ulGpioPin);
}


/********************************************************************************/


/********************************************************************************/

void HAL_vGpioRegister(void)
{

    uint16_t            usCount     = 0x00;
    uint16_t            usGpioNum = sizeof(g_pxGpioInfo)/sizeof(GPIO_INFO_T);

    GPIO_InitTypeDef   xIoConf;
    RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);

    for (; usCount < usGpioNum; usCount++)
    {

        xIoConf.GPIO_Pin   = g_pxGpioInfo[usCount].ulGpioPin;
        xIoConf.GPIO_Mode  = g_pxGpioInfo[usCount].ulGpioMode;
        xIoConf.GPIO_PuPd  = g_pxGpioInfo[usCount].ulGpioPupd;

        //PAD_PullCtrl(xIoConf.GPIO_Pin, xIoConf.GPIO_PuPd);  //前后都设置一下
        GPIO_Init(&xIoConf);
        //PAD_PullCtrl(xIoConf.GPIO_Pin, xIoConf.GPIO_PuPd); // 前后都设置一下
    }
}


