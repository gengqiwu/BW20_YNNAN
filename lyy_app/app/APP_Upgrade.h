/** @file APP_Upgrade.h @brief 升级事件结构及目标派发接口。 */
#ifndef __APP_UPGRADE_H__
#define __APP_UPGRADE_H__

#include <stdint.h>

enum 
{
    APP_UPGRADE_NETWORK_DEV,            /* 联网设备 */
    APP_UPGRADE_BOARD,                  /* 主板 */
};

/** @brief 镜像目标枚举；DEVICE=旧 CD 主板，ZCJ=STM32 D6/0E，不能混用。 */
enum
{
    APP_UPGRADE_APP,
    APP_UPGRADE_SDK,
    APP_UPGRADE_DEVICE,
    APP_UPGRADE_SCREEN,
    APP_UPGRADE_ZCJ,
};

enum 
{

    APP_UPGRADE_TCP,
    APP_UPGRADE_HTTP,
};



/** @brief 事件携带的升级参数；pucData 指向可释放的 NUL 结尾 URL。 */
typedef struct 
{

    uint8_t         ucDevType;
    uint8_t         ucUpgradeType;  
    uint8_t         ucDataType;     
    uint16_t        usDataLen; /**< URL 输入长度，派发时要求非零。 */
    uint8_t         *pucData; /**< URL 缓冲，由 APP_UpgradeDel() 释放并置 NULL。 */
}APP_UPGRADE_INFO_T;



/** @brief 预留初始化入口，当前不创建任务或资源。 */
extern void APP_UpgradeInit(void);
/**
 * @brief 处理并消费升级事件内的 URL 缓冲。
 * @param[in,out] pucData APP_UPGRADE_INFO_T 缓冲，内部 URL 被释放，外层结构不释放。
 * @param[in] ulLen 可读结构字节数，至少 sizeof(APP_UPGRADE_INFO_T)。
 */
extern void APP_UpgradeDel(uint8_t *pucData, uint32_t ulLen);
/**
 * @brief 派发非空 HTTP URL，目标完成状态由下层回调处理。
 * @param[in] ucUpgradeType 当前仅处理 APP_UPGRADE_HTTP。
 * @param[in] ucDataType APP_UPGRADE_APP、APP_UPGRADE_DEVICE 或 APP_UPGRADE_ZCJ。
 * @param[in] pucData NUL 结尾 URL，派发后调用者仍拥有该缓冲。
 * @param[in] ulLen URL 输入长度，必须非零。
 */
extern void APP_UpgradeDelNetWorkDev(uint8_t ucUpgradeType, uint8_t ucDataType, uint8_t *pucData, uint32_t ulLen);

#endif



