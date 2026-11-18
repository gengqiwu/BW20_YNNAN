#include <stdlib.h>
#include "HAL_Ota.h"
#include "APP_Upgrade.h"
#include "lyy_device.h"
#include "lyy_mcu_ota.h"

static void APP_vMcuOtaSta(uint8_t success)
{
    if (success) lyy_mcu_ota_start();
}

static void APP_vOtaSta(uint8_t success)
{
    if (success) board_ota_status_send();
}

void APP_UpgradeDelNetWorkDev(uint8_t type, uint8_t data_type, uint8_t *data, uint32_t len)
{
    if (!data || !len || type != APP_UPGRADE_HTTP) return;
    if (data_type == APP_UPGRADE_APP) HAL_vOtaHttp((const char *)data, NULL);
    else if (data_type == APP_UPGRADE_DEVICE) HAL_vOtaDeviceHttp((const char *)data, APP_vOtaSta);
    else if (data_type == APP_UPGRADE_ZCJ) HAL_vOtaDeviceHttp((const char *)data, APP_vMcuOtaSta);
}

void APP_UpgradeDel(uint8_t *data, uint32_t len)
{
    if (!data || len < sizeof(APP_UPGRADE_INFO_T)) return;
    APP_UPGRADE_INFO_T *info = (APP_UPGRADE_INFO_T *)data;
    if (info->ucDevType == APP_UPGRADE_BOARD)
        APP_UpgradeDelNetWorkDev(info->ucUpgradeType, info->ucDataType, info->pucData, info->usDataLen);
    free(info->pucData);
    info->pucData = NULL;
}

void APP_UpgradeInit(void) {}
