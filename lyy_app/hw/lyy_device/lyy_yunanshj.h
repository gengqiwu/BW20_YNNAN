/*
 * @author: wu gengqi
 * @Date: 2026-06-11 
 * @LastEditors: wu gengqi
 * @LastEditTime: 2026-06-11
 * @Description: 宇楠售货机设备驱动
 */

#ifndef __LYY_YUNANSHJ_H__
#define __LYY_YUNANSHJ_H__
#include <stdint.h>


extern int device_yunanshj_pay_apply(void);
extern int device_yunanshj_pay_statr(void);
extern void device_yunanshj_packet_parse(uint8_t *data, uint16_t len);

#endif