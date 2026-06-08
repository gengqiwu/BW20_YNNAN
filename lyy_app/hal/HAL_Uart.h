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

};

typedef void (*vRecvFunc)(uint8_t *pucData, uint32_t ulLen);


extern void HAL_vUartInit(uint8_t ucPort, uint32_t ulBaud);
extern void HAL_vUartSetParam(uint8_t ucPort,uint32_t ulBaud);
extern void HAL_vUartRecvProc(uint8_t ucPort, vRecvFunc xRecvFunc);
extern void HAL_vUartSend(uint8_t ucPort, uint8_t *pucData, uint32_t ulLen, uint32_t ulTimeout);



#endif





