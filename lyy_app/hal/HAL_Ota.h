/** @file HAL_Ota.h @brief BW20 自升级与 STM32 固件下载缓存接口。 */
/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-11-09 14:03:53
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-08 14:02:46
 * @FilePath: \lyy_app\main\hal\HAL_Ota.h
 */
#ifndef __HAL_OTA_H__
#define __HAL_OTA_H__
#include <stdint.h>

/** @brief STM32 文件缓存虚拟基址；不是 BW20 物理 Flash 地址，禁止直接擦写芯片此地址。 */
#define FILE_NAME_HW_DEVICE_OTA_LOAD (0x003A0000U)
/** @brief 缓存镜像长度上限 0x46000 字节，下载和分包读取都必须遵守。 */
#define FILE_NAME_HW_DEVICE_OTA_SIZE (0x46000U)
/** @brief VFS_REGION_1 中保存下载镜像的文件名。 */
#define HAL_BOARD_OTA_FILE "zcj_board_ota.bin"


/**
 * @brief 升级操作状态回调。
 * @param[in] ucSta 1 表示当前操作成功，0 表示失败；成功所处阶段由调用接口定义。
 */
typedef void (*vOtaStaFunc)(uint8_t ucSta);

/**
 * @brief 复制 URL 并创建 Realtek 自升级任务，成功后切换镜像并复位。
 * @param[in] pcUrl 以 NUL 结尾的 HTTP/HTTPS 镜像地址；函数返回后调用方可释放。
 * @param[in] xOtaStaFunc 可为空的状态回调，任务结束或前置检查失败时调用。
 * @note 忙时拒绝本次操作；成功回调表示 SDK 升级完成，不是重启后的业务验收。
 */
extern void HAL_vOtaHttp(const char *pcUrl, vOtaStaFunc xOtaStaFunc);
/**
 * @brief 异步下载对端固件到文件缓存，完整下载且长度一致后通知调用者。
 * @param[in] url NUL 结尾 HTTP/HTTPS 地址，HTTP 层在启动任务前复制主机名与路径。
 * @param[in] callback 下载状态回调，可为 NULL；此接口本身不发送 STM32 升级块。
 * @pre board_ota_param_t.file_size 已设置为期望镜像字节数。
 * @note 下载成功不代表 STM32 已升级；后续由 APP 升级回调选择 CD 或 D6/0E 分包链路。
 */
extern void HAL_vOtaDeviceHttp(const char *url, vOtaStaFunc callback);


#endif


