/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:12:27
 * @Description: 
 */
#ifndef __LYY_LGXYJMAX_H__
#define __LYY_LGXYJMAX_H__

#include "stdint.h" 
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

extern int lg13kg_wsm_jyj_netty_server_data_handle(uint8_t *pack, uint16_t len);
extern int lg13kg_wsm_jyj_device_uart_data_handle(uint8_t *pack, uint16_t len);
extern void lg13kg_wsm_jyj_device_task_init(void);

#endif



