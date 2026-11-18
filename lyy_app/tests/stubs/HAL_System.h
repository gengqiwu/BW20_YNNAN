#ifndef TEST_HAL_SYSTEM_H
#define TEST_HAL_SYSTEM_H
#include <stdint.h>
#include <stddef.h>
typedef void *SemaphoreHandle_t;
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffU
struct HalSoftTimer {
    unsigned long ullSysTick, ullRepeat;
    void (*vTimeoutCb)(void *);
    struct HalSoftTimer *pxNext;
    void *pxArg;
};
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t, uint32_t);
void xSemaphoreGive(SemaphoreHandle_t);
uint32_t HAL_ulGetSysTick(void);
uint8_t HAL_ucTickTimeout(uint32_t, uint32_t);
void HAL_vReset(char *, uint32_t);
void HAL_vSoftTimeInit(struct HalSoftTimer *, void (*)(void *), unsigned long, void *);
#endif
