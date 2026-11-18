/**
 * @file HAL_Gpio.c
 * @brief 按纸钞盒原理图注册模块本地灯、PA27 按键与 PA26 配网模式输入。
 * @note 机台和现金脉冲引脚由 STM32 管理，不在本模块配置。
 */
#include <stdint.h>
#include "ameba_soc.h"
#include "HAL_Gpio.h"

/* BW20-WIFI_Cash_Box_STHW V1.00, sheet 2. Pulse IO is owned by STM32. */
typedef struct { uint32_t pin, mode, pull; } GPIO_INFO_T;
static const GPIO_INFO_T gpio_info[] = {
    [GPIO_LED_RED] = { _PB_18, GPIO_Mode_OUT, GPIO_PuPd_NOPULL },
    [GPIO_LED_GREEN] = { _PB_17, GPIO_Mode_OUT, GPIO_PuPd_NOPULL },
    [GPIO_NETWORK_TRIGGER] = { _PA_27, GPIO_Mode_IN, GPIO_PuPd_UP },
    [GPIO_MODE_CTRL] = { _PA_26, GPIO_Mode_IN, GPIO_PuPd_NOPULL },
};

/**
 * @brief 判断旧 GPIO 枚举项是否在当前纸钞盒上有本地映射。
 * @param[in] type 逻辑 GPIO 枚举值。
 * @return 非零表示可访问；0 表示未映射，不能据此索引 gpio_info。
 */
static int valid_gpio(uint32_t type)
{
    return type == GPIO_LED_RED || type == GPIO_LED_GREEN ||
           type == GPIO_NETWORK_TRIGGER || type == GPIO_MODE_CTRL;
}

/**
 * @brief 写入已映射引脚，未映射的旧脉冲控制枚举被忽略。
 * @param[in] type 逻辑 GPIO 枚举值。
 * @param[in] level 输出电平。
 */
void HAL_vGpioSet(uint32_t type, uint8_t level)
{
    if (valid_gpio(type)) GPIO_WriteBit(gpio_info[type].pin, level);
}

/**
 * @brief 读取本地映射引脚电平。
 * @param[in] type 逻辑 GPIO 枚举值。
 * @return 已映射引脚的实际电平；未映射项返回 GPIO_HIGH。
 */
uint8_t HAL_vGpioGet(uint32_t type)
{
    return valid_gpio(type) ? GPIO_ReadDataBit(gpio_info[type].pin) : GPIO_HIGH;
}

/** @brief 打开 GPIO 时钟，只初始化本地有效映射，避免接管 STM32 外设引脚。 */
void HAL_vGpioRegister(void)
{
    RCC_PeriphClockCmd(APBPeriph_GPIO, APBPeriph_GPIO_CLOCK, ENABLE);
    for (uint32_t i = 0; i < sizeof(gpio_info) / sizeof(gpio_info[0]); ++i) {
        if (!valid_gpio(i)) continue;
        GPIO_InitTypeDef cfg = {0};
        cfg.GPIO_Pin = gpio_info[i].pin;
        cfg.GPIO_Mode = gpio_info[i].mode;
        cfg.GPIO_PuPd = gpio_info[i].pull;
        GPIO_Init(&cfg);
    }
}
