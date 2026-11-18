/** @file APP_Led.h @brief 模块本地灯控制及供 STM32 轮询的系统状态码。 */
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

/** @brief 随 D6/0B 下发的系统状态编号，与 STM32 保持一致，禁止随意重排。 */
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
/** @brief 获取当前缓存的系统灯状态。 @return LED_STATUS_* 编号，不是 GPIO 电平。 */
extern uint8_t get_led_status(void);
/**
 * @brief 更新供 STM32 配置轮询读取的系统状态缓存，不直接发送串口命令。
 * @param[in] status LED_STATUS_* 编号；当前不执行范围检查。
 */
extern void set_led_status(uint8_t status);

extern void APP_vLedInit(void);
extern void APP_vLedSstSta(uint8_t ucLedType, uint8_t ucLedSta);


#endif


