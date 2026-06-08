/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-11-28 15:39:05
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-16 09:53:37
 * @FilePath: \ameba-rtos\lyy_app\app\APP_Param.h
 */
#ifndef __APP_PARAM_H__
#define __APP_PARAM_H__

#include "HAL_Wifi.h"

#pragma pack(push, 1)

typedef struct {

#define MAX_ID_NUM              				(32U)

    char  cInitMark;		
    char  pcDevId[MAX_ID_NUM];

}APP_DEV_INFO_T;


typedef struct {

    uint32_t ulGiftOutSum;
    uint32_t ulCoinInSum;
    uint32_t ulCoinOutSum;
}APP_ACC_T;


typedef struct {

    uint32_t ulPulsetype;
    uint32_t ulPulseWidth;
    uint32_t ulPulseSpace;
}APP_PULSE_T;


typedef struct {

    char                cInitMark;
    uint32_t            ulBaudRate;
    WIFI_INFO_T         xWifiInfo;
    APP_ACC_T           xDevAcc;
    APP_PULSE_T         xDevPulse;
}APP_PARAM_T;


typedef struct {

    char                cInitMark;
    char                pcProductKey[64];
    char                pcProductSecret[64];
    uint16_t            ulPort;
    char                pcSerUrl[128];
    char                pcDevType[64];   
}APP_SER_T;


#pragma pack(pop)


/*  */
extern int          APP_lParamInit(void);
extern int          APP_vDevInfoInit(const char *pcDevId);

/* 设备参数信息 */
extern int          APP_lSetParamUpdate(void);
extern char         *APP_pcGetParamDevId(void);
extern uint32_t     APP_ulGetParamBaudRate(void);
extern int          APP_lParamSetInfo(APP_PARAM_T *pxAppParam);
extern int          APP_lParamSetBaundRate(uint32_t ulBaundRate);
extern int          APP_lPulseParamSetInfo(uint32_t pxAppParam);

/* WIFI信息 */
extern WIFI_MODE_T  APP_xGetParamWifiMode(void);
extern char         *APP_pcGetParamWifiPwd(void);
extern char         *APP_pcGetParamWifiSsid(void);
extern void         APP_vSetParamWifiMode(WIFI_MODE_T xWifiMode);
extern int          APP_lParamSetWifiInfo(WIFI_INFO_T *pcWifiInfo);


/* 账目 */
extern APP_ACC_T    *APP_pxlGetParameAcc(void);

/* 机台参数 */
extern APP_PULSE_T  *APP_pxlGetParamePulse(void);
extern int          APP_lParamSetParamePulse(APP_PULSE_T *pxApluse);


/* 连接服务的信息 */
extern int          APP_lSerSetInfo(APP_SER_T *pxAppSer);
extern char         *APP_pcGetSerUrl(void);
extern uint16_t     APP_usGetSerPort(void);
extern char         *APP_pcGetProductKey(void);
extern char         *APP_pcGetProductSecret(void);
extern char         *APP_pcGetDevType(void);
extern int          APP_lserSetDevTypeInfo(char *pcDevType);


#endif





