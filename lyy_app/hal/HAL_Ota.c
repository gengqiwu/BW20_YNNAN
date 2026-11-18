/** @file HAL_Ota.c @brief 异步网络升级：Realtek 本机镜像与文件缓存的对端镜像下载。 */
#include <stdio.h>
#include <string.h>
#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_Http.h"
#include "HAL_System.h"
#include "HAL_Flash.h"
#include "lyy_device.h"
#include "sys_api.h"
#include "lyy_mcu_ota.h"
#include "ameba_ota.h"

static uint8_t ota_busy;
static vOtaStaFunc ota_callback;

/**
 * @brief 解析私有 URL 副本，执行 SDK OTA，释放资源并在成功时复位。
 * @param[in,out] arg 堆分配的可写 URL 副本，由本任务释放。
 * @note 状态回调在删除任务之前调用，失败时恢复空闲；成功会清旧 OTA 签名并复位。
 */
static void app_ota_thread(void *arg)
{
    char *url = arg;
    ota_context *ctx = NULL;
    int result = -1;
    uint16_t port;
    char *host, *path;
    if (!strncmp(url, "http://", 7)) { port = 80; host = url + 7; }
    else if (!strncmp(url, "https://", 8)) { port = 443; host = url + 8; }
    else goto done;
    path = strchr(host, '/');
    if (!path) goto done;
    *path++ = 0;
    ctx = pvPortMalloc(sizeof(*ctx));
    if (!ctx) goto done;
    memset(ctx, 0, sizeof(*ctx));
    result = ota_update_init(ctx, host, port, path, port == 80 ? OTA_HTTP : OTA_HTTPS);
    if (!result) result = ota_update_start(ctx);
    ota_update_deinit(ctx);
    vPortFree(ctx);
 done:
    vPortFree(url);
    if (ota_callback) ota_callback(result == 0);
    ota_busy = 0;
    if (!result) {
        sys_clear_ota_signature();
        HAL_vReset("OTA update complete", 50);
    }
    vTaskDelete(NULL);
}

/** @brief 启动本机升级任务，URL 所有权和回调语义见 HAL_Ota.h。 */
void HAL_vOtaHttp(const char *url, vOtaStaFunc cb)
{
    if (!url || ota_busy || lyy_mcu_ota_busy()) { if (cb) cb(0); return; }
    size_t len = strlen(url);
    char *copy = pvPortMalloc(len + 1);
    if (!copy) { if (cb) cb(0); return; }
    memcpy(copy, url, len + 1);
    ota_callback = cb;
    ota_busy = 1;
    if (xTaskCreate(app_ota_thread, "OTA_THREAD", 16 * 1024 / sizeof(portSTACK_TYPE), copy, 10, NULL) != pdPASS) {
        vPortFree(copy);
        ota_busy = 0;
        if (cb) cb(0);
    }
}

static uint32_t board_offset;
static uint8_t board_write_error;
static vOtaStaFunc board_callback;

/**
 * @brief HTTP body 分片回调，将固件顺序写到对端镜像缓存。
 * @param[in] data 当前分片，不保证 NUL 结尾。
 * @param[in] len 分片字节数。
 * @note 超过镜像缓存容量或写入失败后置错误标记，忽略后续分片，不推进成功偏移。
 */
static void board_ota_data(char *data, uint32_t len)
{
    if (board_write_error) return;
    if (len > FILE_NAME_HW_DEVICE_OTA_SIZE - board_offset ||
        HAL_lFlashAddrWrite(FILE_NAME_HW_DEVICE_OTA_LOAD + board_offset, (uint8_t *)data, len)) {
        board_write_error = 1;
        return;
    }
    board_offset += len;
}

/**
 * @brief HTTP 结束回调，检查下载状态、写入状态及实际/期望长度。
 * @param[in] result HTTP_GET_SUCCESS 或 HTTP_GET_ERR。
 * @param[in] len HTTP 层统计的实际 body 字节数。
 * @note 先清下载忙标记再通知调用者，以便回调启动 STM32 分包任务。
 */
static void board_ota_result(uint8_t result, uint32_t len)
{
    uint8_t success = result == HTTP_GET_SUCCESS && !board_write_error &&
        board_offset && len == board_offset && board_offset == board_ota_param_t.file_size;
    ota_busy = 0;
    if (board_callback) board_callback(success);
    if (!success) LYY_LOGE("STM32 OTA download failed: received=%lu expected=%lu", board_offset, board_ota_param_t.file_size);
}

/** @brief 启动对端固件下载，前置条件和完成语义见 HAL_Ota.h。 */
void HAL_vOtaDeviceHttp(const char *url, vOtaStaFunc cb)
{
    if (!url || ota_busy || lyy_mcu_ota_busy() || !board_ota_param_t.file_size ||
        board_ota_param_t.file_size > FILE_NAME_HW_DEVICE_OTA_SIZE) {
        if (cb) cb(0);
        return;
    }
    ota_busy = 1;
    board_offset = 0;
    board_write_error = 0;
    board_callback = cb;
    if (HAL_lFlashAddrErase(FILE_NAME_HW_DEVICE_OTA_LOAD, FILE_NAME_HW_DEVICE_OTA_SIZE)) {
        board_ota_result(HTTP_GET_ERR, 0);
        return;
    }
    HAL_vHttpGet(url, board_ota_data, board_ota_result);
}
