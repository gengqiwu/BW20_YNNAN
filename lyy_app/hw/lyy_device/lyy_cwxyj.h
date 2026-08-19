/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:12:27
 * @Description: 
 */
#ifndef __LYY_CWXYJ_H__
#define __LYY_CWXYJ_H__

#include "stdint.h" 
#include "stdio.h"
#include "stdlib.h"
#include "string.h"


typedef struct{
    uint8_t devSta;//设备状态  0x00空闲  0x01工作中  0x02故障
    uint8_t runMode;//洗衣模式
    uint8_t runSta;//洗衣状态
    uint8_t devErrCode;//洗衣机故障码
}TYPE_SKYWORTH_XYJ_DEV_STA;

extern void lyy_device_cwxyj_init(void);
extern int skyworth_xyj_pay_start(uint16_t mode);
extern int skyworth_qhxyj_pay_apply(void);
extern uint8_t CYJYJ_start(uint8_t data1, uint8_t data2);
extern void skyworth_xyj_packet_parse(uint8_t *package, uint16_t len);
extern int terminal_skyworthXYJ_stop(void);

#endif



