/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-11-09 14:03:53
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-08 14:02:46
 * @FilePath: \lyy_app\main\hal\HAL_Ota.h
 */
#ifndef __HAL_OTA_H__
#define __HAL_OTA_H__



typedef void (*vOtaStaFunc)(uint8_t ucSta);

extern void HAL_vOtaHttp(const char *pcUrl, vOtaStaFunc xOtaStaFunc);


#endif


