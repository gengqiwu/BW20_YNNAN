/**
 * @file APP_Detect.c
 * @brief 纸钞盒配网按键与配网模式信号采样，不在 BW20 本地采集现金脉冲。
 */
#include <stdint.h>
#include "HAL_Gpio.h"
#include "HAL_System.h"
#include "APP_Detect.h"

static uint32_t machine_level;
static uint32_t key_tick;
static uint8_t key_down;
static void (*detect_cb[APP_DETECT_MAX])(void *);

/* Cash-box pulse inputs belong to STM32, not the Wi-Fi module. */
/**
 * @brief 缓存 PA26 对应的 WIFI_AIRMOD 电平，用于选择配网方式。
 * @note 保留旧接口名，但此值不是机台投币输出极性，不能用于现金脉冲控制。
 */
void APP_prvDetectMachtineLvl(void)
{
    machine_level = HAL_vGpioGet(GPIO_MODE_CTRL);
}

/** @brief 获取最近采样的配网模式电平。 @return GPIO 低/高电平对应的缓存值。 */
uint32_t APP_ulGetMachtineLvl(void) { return machine_level; }

/**
 * @brief 注册检测事件回调。
 * @param[in] type 小于 APP_DETECT_MAX 的检测索引，越界时忽略。
 * @param[in] cb 回调函数，可为 NULL；本模块实际仅触发配网按键事件，参数为 NULL。
 */
void APP_vDeteclRegisterFunc(uint16_t type, void (*cb)(void *))
{
    if (type < APP_DETECT_MAX) detect_cb[type] = cb;
}

/**
 * @brief 轮询低电平有效的配网按键，持续按下 3 秒后触发配网回调。
 * @note 使用系统毫秒时间差而非轮询次数计时；持续不松手时每 3 秒再次触发。
 *       回调在当前轮询上下文同步执行，松手后清除按下状态。
 */
void APP_prvDetectNetWorkKey(void)
{
    if (HAL_vGpioGet(GPIO_NETWORK_TRIGGER) != GPIO_LOW) {
        key_down = 0;
        return;
    }
    if (!key_down) {
        key_down = 1;
        key_tick = HAL_ulGetSysTick();
    }
    if (HAL_ucTickTimeout(key_tick, 3000)) {
        key_tick = HAL_ulGetSysTick();
        if (detect_cb[APP_NETWORK_DETECT]) detect_cb[APP_NETWORK_DETECT](NULL);
    }
}

/** @brief 保留毫秒检测入口，仅调用配网按键检测，不采集 STM32 负责的投币信号。 */
void APP_vDetecTimerMs(void) { APP_prvDetectNetWorkKey(); }
/** @brief 初始化检测缓存，仅采样配网模式电平，不创建额外任务。 */
void APP_vDetectInit(void) { APP_prvDetectMachtineLvl(); }
