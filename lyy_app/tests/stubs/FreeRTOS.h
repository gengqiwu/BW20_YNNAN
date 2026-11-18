#ifndef TEST_FREERTOS_H
#define TEST_FREERTOS_H
#include "HAL_System.h"
typedef uint32_t portSTACK_TYPE;
#include <stdlib.h>
#define pvPortMalloc malloc
#define vPortFree free
typedef void (*TaskFunction_t)(void *);
#define pdPASS 1
#define pdFALSE 0
#define pdMS_TO_TICKS(ms) (ms)
#define portTICK_PERIOD_MS 1
#endif
