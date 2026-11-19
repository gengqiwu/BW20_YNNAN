/**
 * @file APP_Param.h
 * @brief BW20 参数文件布局和纸钞盒独立输出设置接口。
 * @warning packed 结构直接持久化，修改字段布局前必须设计旧参数迁移，不能照搬 ESP32 裸结构。
 */
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

/** @brief 设备号文件结构；D 标志后保留 32 个 ID 字节及额外 NUL 空间。 */
typedef struct {

/** @brief 最大字符串空间为 33，允许的设备号正文仍为 32 字节。 */
#define MAX_ID_NUM                      (33U)

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


/** @brief 原 BW20 通用参数布局，机台波特率不控制 BW20 与 STM32 的固定速率链路。 */
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

/** @brief 对端输出端口编号；与 START_TYPE_COIN/LEVEL 的软件模式枚举不是同一套值。 */
enum { PULSE_COIN, PULSE_ZCJ_1, PULSE_ZCJ_2 };
/**
 * @brief 独立保存于 ZCJ_PULSE_INFO 的 4 字节输出配置。
 * @note bytes[0] 为 START_TYPE_COIN/LEVEL；bytes[1] 为 COIN/ZCJ1/ZCJ2；其余字节保留。
 */
union U { uint32_t value; uint8_t bytes[4]; };
extern union U ulPulsetype;
/** @brief 读取独立输出配置。 @retval LYY_SUCCESS 读取成功。 @retval LYY_READ_PARAM_FAIL 使用脉冲/ZCJ1默认值。 */
extern int APP_lPulseParaInit(void);
/** @brief 仅检查服务参数内存缓存的初始化标记，不重新读文件。 @return LYY_SUCCESS 或 LYY_READ_PARAM_FAIL。 */
extern int APP_lSerParamInit(void);
/**
 * @brief 业务层参数读取适配。
 * @param[in] file_name VFS 参数文件名。
 * @param[out] data 至少 len 字节的接收缓冲。
 * @param[in] len 请求字节数。
 * @return 实际读取字节数；文件打开失败为 0，调用者应比较完整长度。
 */
extern int32_t lyy_para_read(const char *file_name, uint8_t *data, uint32_t len);
/**
 * @brief 业务层参数整文件写入适配，会截断旧文件。
 * @param[in] file_name VFS 参数文件名。
 * @param[in] data 待保存字节。
 * @param[in] len 保存字节数。
 * @return 实际写入字节数；打开失败为 -1，调用者应比较完整长度。
 */
extern int32_t lyy_para_write(const char *file_name, uint8_t *data, uint32_t len);

/*  */
/**
 * @brief 加载设备号、通用/服务参数及独立输出设置。
 * @retval LYY_SUCCESS 设备号读取有效，缺失的通用/服务配置已尝试按默认值初始化。
 * @retval LYY_READ_PARAM_FAIL 设备号文件大小或初始化标记无效，应进入工厂模式。
 * @note 兼容旧版少一个 NUL 字节的设备号文件；默认参数写入失败不改变本函数成功返回路径。
 */
extern int          APP_lParamInit(void);
/**
 * @brief 写入最多 32 字节的设备号及初始化标记。
 * @param[in] pcDevId NUL 结尾设备号，NULL 或超长时拒绝；字符规则由工厂层校验。
 * @return LYY_SUCCESS、LYY_FAIL 或 LYY_WRITE_PARAM_FAIL。
 */
extern int          APP_vDevInfoInit(const char *pcDevId);

/* 设备参数信息 */
extern int          APP_lSetParamUpdate(void);
extern char         *APP_pcGetParamDevId(void);
extern uint32_t     APP_ulGetParamBaudRate(void);
extern int          APP_lParamSetInfo(APP_PARAM_T *pxAppParam);
extern int          APP_lParamSetBaundRate(uint32_t ulBaundRate);
/**
 * @brief 更新输出配置内存缓存并写入独立参数文件。
 * @param[in] pxAppParam 软件输出模式和端口组成的 4 字节值。
 * @return LYY_SUCCESS 或 LYY_WRITE_PARAM_FAIL；写失败时内存缓存已改变。
 */
extern int          APP_lPulseParamSetInfo(uint32_t pxAppParam);

/* WIFI信息 */
extern WIFI_MODE_T APP_xGetParamWifiMode(void);
extern char         *APP_pcGetParamWifiPwd(void);
extern char         *APP_pcGetParamWifiSsid(void);
/** @brief 持久化 Wi-Fi 模式。写入失败时恢复原内存模式。 */
extern int          APP_lSetParamWifiMode(WIFI_MODE_T xWifiMode);
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





