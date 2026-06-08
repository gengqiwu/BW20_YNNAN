#ifndef __APP_LED_H__
#define __APP_LED_H__

enum
{
    LED_GREEN,
    LED_RED,
    LED_MAX
};


enum
{
    LED_STA_OFF,
    LED_STA_ON,
    LED_STA_SLOW_FLASH,
    LED_STA_QUICK_FLASH,
    LED_STA_MAX   
};


extern void APP_vLedInit(void);
extern void APP_vLedSstSta(uint8_t ucLedType, uint8_t ucLedSta);


#endif


