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

/* Values are part of the STM32 network-status protocol. */
enum {
    LED_STATUS_LOGIN_SUCCESS = 0,
    LED_STATUS_DEFAULT,
    LED_STATUS_START_SYSTEM,
    LED_STATUS_SIM_ERROR,
    LED_STATUS_NET_REGISTER,
    LED_STATUS_DNS_FAIL,
    LED_STATUS_CONNECT_FAIL,
    LED_STATUS_NO_DEVID,
    LED_STATUS_LOGIN_FAIL,
    LED_STATUS_FACTORY_PDP_OK,
    LED_STATUS_WIFI_AP,
    LED_STATUS_WIFI_SMART,
};
extern uint8_t get_led_status(void);
extern void set_led_status(uint8_t status);

extern void APP_vLedInit(void);
extern void APP_vLedSstSta(uint8_t ucLedType, uint8_t ucLedSta);


#endif


