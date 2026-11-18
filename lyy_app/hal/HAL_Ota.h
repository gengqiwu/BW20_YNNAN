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

/* Virtual addresses for the file-backed STM32 firmware cache, not BW20 flash. */
#define FILE_NAME_HW_DEVICE_OTA_LOAD (0x003A0000U)
#define FILE_NAME_HW_DEVICE_OTA_SIZE (0x46000U)
#define HAL_BOARD_OTA_FILE "zcj_board_ota.bin"


typedef void (*vOtaStaFunc)(uint8_t ucSta);

extern void HAL_vOtaHttp(const char *pcUrl, vOtaStaFunc xOtaStaFunc);
extern void HAL_vOtaDeviceHttp(const char *url, vOtaStaFunc callback);


#endif


