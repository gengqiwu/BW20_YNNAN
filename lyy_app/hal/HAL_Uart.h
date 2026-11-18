/**
 * @file HAL_Uart.h
 * @brief BW20 主板 UART 接口，纸钞盒仅由 STM32 代理外设和屏幕通信。
 */
/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-11-29 13:56:01
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-31 18:06:13
 * @FilePath: \ameba-rtos\lyy_app\hal\HAL_Uart.h
 */
#ifndef __HAL_UART_H__
#define __HAL_UART_H__


enum {

    /* 主板通讯口 */
    HAL_UART_MAIN_BOARD,

    /* 串口屏通讯口 */
    HAL_UART_SCREEN,

    HAL_UART_MAX,
    /** @brief SDK 已初始化的 LOG UART，仅用于工厂提示，不是配置数组索引。 */
    HAL_UART_DEBUG = HAL_UART_MAX,

};

/**
 * @brief 串口接收任务的同步回调。
 * @param[in,out] pucData 当前接收块，回调返回后缓冲会复用，异步使用需复制。
 * @param[in] ulLen 实际数据字节数，可能是半帧或多帧，不等于协议帧长。
 */
typedef void (*vRecvFunc)(uint8_t *pucData, uint32_t ulLen);


/**
 * @brief 初始化硬件 UART 接收资源及任务，已初始化的端口直接返回。
 * @param[in] ucPort HAL_UART_MAIN_BOARD 或 HAL_UART_SCREEN；DEBUG 由 SDK 管理。
 * @param[in] ulBaud 初始化波特率，纸钞盒启动流程传 115200。
 * @note 接口不返回资源创建结果，调用方须按初始化顺序使用。
 */
extern void HAL_vUartInit(uint8_t ucPort, uint32_t ulBaud);
/**
 * @brief 调整端口速率和接收空闲阈值。
 * @param[in] ucPort 硬件 UART 索引，非法索引忽略。
 * @param[in] ulBaud 期望速率；MAIN_BOARD 强制使用 115200，防止机台设置破坏模块间链路。
 */
extern void HAL_vUartSetParam(uint8_t ucPort,uint32_t ulBaud);
/**
 * @brief 替换指定硬件串口的接收处理函数。
 * @param[in] ucPort 硬件端口索引，DEBUG/非法索引忽略。
 * @param[in] xRecvFunc 同步接收回调，NULL 表示不处理接收块。
 */
extern void HAL_vUartRecvProc(uint8_t ucPort, vRecvFunc xRecvFunc);
/**
 * @brief 互斥逐字节写入硬件 UART，或直接输出到 LOG UART。
 * @param[in] ucPort 目标端口，DEBUG 不访问硬件 UART 配置数组。
 * @param[in] pucData 可读发送缓冲，逐字节复制到 FIFO，不启动发送 DMA。
 * @param[in] ulLen 发送字节数，0 时忽略。
 * @param[in] ulTimeout 等待 FIFO 可写的毫秒超时，0 使用 1000 ms；不限制获取互斥锁的等待。
 * @note 返回时不再引用调用者缓冲，但 FIFO 数据可能仍在传输；超时会停止剩余字节发送。
 *       此 void 接口不能确认对端接收或业务成功，不得用于 ISR 中阻塞发送。
 */
extern void HAL_vUartSend(uint8_t ucPort, uint8_t *pucData, uint32_t ulLen, uint32_t ulTimeout);



#endif





