/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-10-18 11:15:18
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2023-11-08 10:05:44
 * @FilePath: \lyy_app\main\hal\HAL_Wifi.h
 */
#ifndef __HAL_WIFI_H__
#define __HAL_WIFI_H__


typedef enum 
{
    WIFI_STA,
    WIFI_AP,
    WIFI_APSTA
}WIFI_MODE_T;


typedef struct 
{
    char        pcSsid[64];
    char        pcPwd[64];
    WIFI_MODE_T xMode;
}WIFI_INFO_T;


typedef enum 
{
    WIFI_WORK_IDLE,
    WIFI_STA_CONNING,
    WIFI_STA_CONNED,

    WIFI_AP_CONNED,
    WIFI_AP_DIS_CON,
}WIFI_WORK_STA_T;


typedef void (*vWifiWorkSta)(WIFI_WORK_STA_T xSta);

extern void     HAL_vWifiLwipDhcpInit(void);
extern int      HAL_lGetWifiRssiToCsq(void);
extern uint32_t HAL_ulGetBroadcastAddr(void);
extern void     HAL_vWifiInit(WIFI_INFO_T *pxWifiInfo, vWifiWorkSta xFunc);

extern void HAL_wifista_connect_flash_wifi(void);
extern void HAL_wifista_connect_default_wifi(void);
extern void HAL_wifista_again_connect_wifi(WIFI_INFO_T *pxWifiInfo);

#endif
