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


extern int device_yunanshj_pay_apply(uint8_t channel);
extern int device_yunanshj_pay_start(void);
extern void yunanshj_cmd_set_dispense_detect(uint8_t data);
extern void yunanshj_cmd_query_dispense_detect_status(void);
extern void yunanshj_dispense_detect_query_report_pending(void);
extern void device_yunanshj_packet_parse(uint8_t *data, uint16_t len);
extern void device_yunanshj_stream_parse(uint8_t *data, uint16_t len);

#endif
