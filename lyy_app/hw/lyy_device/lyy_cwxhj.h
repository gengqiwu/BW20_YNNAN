/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:12:27
 * @Description: 
 */
#ifndef __LYY_CWXHJ_H__
#define __LYY_CWXHJ_H__

#include "stdint.h" 
#include "stdio.h"
#include "stdlib.h"
#include "string.h"


typedef struct{
    uint8_t devSta;//设备状态  0x00空闲  0x01工作中  0x02故障
    uint8_t runMode;//烘干模式
    uint8_t runSta;//
    uint8_t devErrCode;//
}TYPE_SKYWORTH_XHJ_DEV_STA;

extern void lyy_device_cwxhj_init(void);
extern int skyworth_xhj_pay_start(uint16_t mode);
extern int skyworth_qhxhj_pay_apply(void);
extern void skyworth_xhj_packet_parse(uint8_t *package, uint16_t len);

#endif



