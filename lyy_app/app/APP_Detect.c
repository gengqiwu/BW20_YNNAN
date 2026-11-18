#include <stdint.h>
#include "HAL_Gpio.h"
#include "HAL_System.h"
#include "APP_Detect.h"

static uint32_t machine_level;
static uint32_t key_tick;
static uint8_t key_down;
static void (*detect_cb[APP_DETECT_MAX])(void *);

/* Cash-box pulse inputs belong to STM32, not the Wi-Fi module. */
void APP_prvDetectMachtineLvl(void)
{
    machine_level = HAL_vGpioGet(GPIO_MODE_CTRL);
}

uint32_t APP_ulGetMachtineLvl(void) { return machine_level; }

void APP_vDeteclRegisterFunc(uint16_t type, void (*cb)(void *))
{
    if (type < APP_DETECT_MAX) detect_cb[type] = cb;
}

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

void APP_vDetecTimerMs(void) { APP_prvDetectNetWorkKey(); }
void APP_vDetectInit(void) { APP_prvDetectMachtineLvl(); }
