/** @file APP_Upgrade.c @brief 按镜像目标选择 BW20 自升级、旧 CD 主板升级或 STM32 D6/0E 升级。 */
#include <stdlib.h>
#include "HAL_Ota.h"
#include "APP_Upgrade.h"
#include "lyy_device.h"
#include "lyy_mcu_ota.h"

/**
 * @brief 纸钞主控镜像下载完成后启动 D6/0E 分包任务。
 * @param[in] success 非零表示下载缓存通过大小及写入检查；零时不启动传输。
 */
static void APP_vMcuOtaSta(uint8_t success)
{
    if (success) lyy_mcu_ota_start();
}

/**
 * @brief 旧主板镜像下载完成后发出第一个 CD 数据块。
 * @param[in] success 非零表示缓存可读；后续块由主板 ACK 驱动。
 */
static void APP_vOtaSta(uint8_t success)
{
    if (success) board_ota_status_send();
}

/**
 * @brief 根据数据目标派发 HTTP 升级，不阻塞等待镜像安装完成。
 * @param[in] type 升级传输方式，当前只处理 APP_UPGRADE_HTTP。
 * @param[in] data_type APP/DEVICE/ZCJ 分别选择本机/CD 主板/D6 纸钞主控。
 * @param[in] data NUL 结尾 URL，不可为空。
 * @param[in] len 输入非零长度；底层仍按字符串终止符读取 URL。
 * @note 不支持的目标或传输方式直接忽略；此 void 接口不返回启动是否成功。
 */
void APP_UpgradeDelNetWorkDev(uint8_t type, uint8_t data_type, uint8_t *data, uint32_t len)
{
    if (!data || !len || type != APP_UPGRADE_HTTP) return;
    if (data_type == APP_UPGRADE_APP) HAL_vOtaHttp((const char *)data, NULL);
    else if (data_type == APP_UPGRADE_DEVICE) HAL_vOtaDeviceHttp((const char *)data, APP_vOtaSta);
    else if (data_type == APP_UPGRADE_ZCJ) HAL_vOtaDeviceHttp((const char *)data, APP_vMcuOtaSta);
}

/**
 * @brief 从升级事件结构派发目标后释放事件内的 URL 缓冲。
 * @param[in,out] data 可写 APP_UPGRADE_INFO_T，pucData 须可由 free() 释放。
 * @param[in] len 结构缓冲字节数，短于结构时不处理。
 * @note 只对 APP_UPGRADE_BOARD 派发；结构有效时即使目标不派发也释放内部 pucData，
 *       结构自身由外部事件系统管理，不能把两层所有权混淆。
 */
void APP_UpgradeDel(uint8_t *data, uint32_t len)
{
    if (!data || len < sizeof(APP_UPGRADE_INFO_T)) return;
    APP_UPGRADE_INFO_T *info = (APP_UPGRADE_INFO_T *)data;
    if (info->ucDevType == APP_UPGRADE_BOARD)
        APP_UpgradeDelNetWorkDev(info->ucUpgradeType, info->ucDataType, info->pucData, info->usDataLen);
    free(info->pucData);
    info->pucData = NULL;
}

/** @brief 预留升级初始化入口，当前无额外初始化动作。 */
void APP_UpgradeInit(void) {}
