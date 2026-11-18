/**
 * @file APP_NetWork.h
 * @brief 声明网络客户端状态、TLS 写入及连接暂停与关闭接口。
 */
/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-11-26 16:21:46
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-11-28 10:46:51
 * @FilePath: \ameba-rtos\lyy_app\app\APP_NetWork.h
 */
#ifndef __APP_NETWORK_H__
#define __APP_NETWORK_H__

/**
 * @brief 客户端连接状态，暂停用于 HTTP/OTA 临时让出连接处理。
 * @note 暂停不是断开；从最近设置暂停起满 10 分钟后，客户端任务转为断开并恢复重连。
 */
typedef enum {
    
    CLIENT_DIS_CON,
    CLIENT_CONNING,
    CLIENT_CONED,
    CLIENT_ISPAUSED
}APP_CLIENT_STA_T;


extern void             APP_vNetWorkInit(void);
extern APP_CLIENT_STA_T APP_pxGetClientSta(void);
extern int              APP_lClientWrite(char *pcBuf, int lLen);
/**
 * @brief 设置客户端状态；每次设置暂停都会刷新 10 分钟恢复计时。
 * @param[in] xSta 目标状态；本接口不负责关闭 TLS 句柄。
 * @note 不提供并发互斥，调用方须协调状态与连接的使用。
 */
extern void             APP_pxSetClientSta(APP_CLIENT_STA_T xSta);
/**
 * @brief 关闭并清空现有 TLS 句柄，有连接时延时约 300 毫秒。
 * @note 不切换客户端状态；调用方须协调并发读写，避免自动重连与手动关闭冲突。
 */
extern void             APP_prvClientCloseSocket(void);



#endif

