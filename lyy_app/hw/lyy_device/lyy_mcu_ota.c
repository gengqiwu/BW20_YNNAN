#include <string.h>
#include "HAL_System.h"
#include "HAL_Flash.h"
#include "HAL_Ota.h"
#include "HAL_Log.h"
#include "lyy_device.h"
#include "lyy_zcj.h"
#include "lyy_mcu_ota.h"

static SemaphoreHandle_t ack_sem;
static volatile uint8_t active;
static uint32_t expected_offset, expected_len, expected_total;
static char expected_name[20];

int lyy_mcu_ota_busy(void) { return active; }

void lyy_mcu_ota_ack(const uint8_t *frame, uint16_t length)
{
    ZJC_OPEN_UPDATE_RES_T ack;
    if (!active || !ack_sem || length != sizeof(ack) + 6) return;
    memcpy(&ack, frame + 4, sizeof(ack));
    if (ack.subCmd != ZCJ_SUB_CMD_DEV_UPGRADE || ack.result ||
        ack.fileTotal != expected_total || ack.fileOffset != expected_offset ||
        ack.upDataLen != expected_len || memcmp(ack.fileName, expected_name, sizeof(expected_name))) return;
    xSemaphoreGive(ack_sem);
}

static void mcu_ota_thread(void *arg)
{
    (void)arg;
    uint8_t buffer[1024];
    uint8_t complete = 0;
    while (expected_offset < expected_total) {
        expected_len = expected_total - expected_offset;
        if (expected_len > sizeof(buffer)) expected_len = sizeof(buffer);
        if (HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD + expected_offset, buffer, expected_len)) break;
        xSemaphoreTake(ack_sem, 0);
        zcj_packet_dev_upgrade(expected_name, expected_total, expected_offset, (char *)buffer, expected_len);
        if (xSemaphoreTake(ack_sem, pdMS_TO_TICKS(5000)) != pdTRUE) {
            LYY_LOGE("STM32 OTA ACK timeout at %lu", expected_offset);
            break;
        }
        expected_offset += expected_len;
    }
    complete = expected_offset == expected_total;
    LYY_LOGI("STM32 OTA transfer %s (%lu/%lu); reboot/version confirmation is required",
        complete ? "acknowledged" : "failed", expected_offset, expected_total);
    active = 0;
    vTaskDelete(NULL);
}

int lyy_mcu_ota_start(void)
{
    if (active || !board_ota_param_t.file_size || board_ota_param_t.file_size > FILE_NAME_HW_DEVICE_OTA_SIZE) return 0;
    if (!ack_sem) ack_sem = xSemaphoreCreateBinary();
    if (!ack_sem) return 0;
    expected_offset = 0;
    expected_total = board_ota_param_t.file_size;
    memset(expected_name, 0, sizeof(expected_name));
    /* Keep the reference 20-byte file-name limit, with a local NUL terminator. */
    memcpy(expected_name, board_ota_param_t.version, strnlen(board_ota_param_t.version, sizeof(expected_name) - 1));
    active = 1;
    if (xTaskCreate(mcu_ota_thread, "STM32_OTA", 6 * 1024 / sizeof(portSTACK_TYPE), NULL, 7, NULL) != pdPASS) {
        active = 0;
        return 0;
    }
    return 1;
}
