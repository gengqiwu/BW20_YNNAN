/**
 * @file lyy_mcu_ota.c
 * @brief 以 1024 字节为块向 STM32 发送缓存固件，每块等待匹配 ACK 后前进。
 * @note 对端要求严格递进偏移，超时或缓存读失败时停止，不自动重发已可能写入的块。
 */
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

/** @brief 返回当前传输任务忙状态，详细语义见 lyy_mcu_ota.h。 */
int lyy_mcu_ota_busy(void) { return active; }

/** @brief 验证当前块 ACK 后释放信号量，帧要求见 lyy_mcu_ota.h。 */
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

/**
 * @brief 独立传输任务：读取缓存、清旧信号、发块并等待最多 5 秒。
 * @param[in] arg 任务参数，未使用。
 * @note 所有块确认后仅表示传输已确认，仍须等待 STM32 复位并核对运行版本。
 *       结束时清忙标记并删除当前任务，信号量保留供后续升级复用。
 */
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

/** @brief 创建单次传输任务并初始化应答匹配字段，返回契约见 lyy_mcu_ota.h。 */
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
