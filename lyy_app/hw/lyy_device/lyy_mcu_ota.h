#ifndef LYY_MCU_OTA_H
#define LYY_MCU_OTA_H
#include <stdint.h>
int lyy_mcu_ota_start(void);
void lyy_mcu_ota_ack(const uint8_t *frame, uint16_t length);
int lyy_mcu_ota_busy(void);
#endif
