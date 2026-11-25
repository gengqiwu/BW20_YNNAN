#ifndef __HAL_GPIO_H__
#define __HAL_GPIO_H__

#include <stdint.h>

enum {

    /* LED  */
    GPIO_LED_RED,
    GPIO_LED_GREEN,

    /* 配网按键 */
    GPIO_NETWORK_TRIGGER,

    /* 常开、常闭检测控制 */
    GPIO_COIN_CTRL,
    GPIO_MODE_CTRL,

    GPIO_GIFT_OUT,  /* 出礼脉冲输出 */
    GPIO_COIN_IN,   /* 投币脉冲输入 */

    /* 机台口 */
    GPIO_COIN_OUT,

};


enum {
    
    GPIO_LOW,
    GPIO_HIGH
};


extern void     HAL_vGpioRegister(void);
extern void     HAL_vGpioSet(uint32_t ulIoType, uint8_t ucLevel);
extern void     HAL_vGpioEnableOutput(uint32_t ulIoType, uint8_t ucIdle);

extern uint8_t  HAL_vGpioGet(uint32_t ulIoType);


#endif


