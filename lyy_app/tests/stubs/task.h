#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
int xTaskCreate(TaskFunction_t, const char *, uint32_t, void *, unsigned, void *);
void vTaskDelete(void *);
void vTaskDelay(uint32_t);
#endif
