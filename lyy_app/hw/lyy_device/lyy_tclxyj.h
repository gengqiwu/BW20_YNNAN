/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:12:27
 * @Description: 
 */
#ifndef __LYY_TCLXYJ_H__
#define __LYY_TCLXYJ_H__

#include "stdint.h" 
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

extern int device_tclxyj_pay_apply(void);
extern int device_tclxyj_pay_start(uint16_t mode);
extern void tclxyj_packet_parse(uint8_t *package, uint16_t len);

#endif



