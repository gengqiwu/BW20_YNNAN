/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:12:27
 * @Description: 
 */
#ifndef __LYY_MSHGJ_H__
#define __LYY_MSHGJ_H__

#include "stdint.h" 
#include "stdio.h"
#include "stdlib.h"
#include "string.h"


typedef struct{
    uint8_t devSta;//设备状态  0x00空闲  0x01工作中
    uint8_t runMode;//
    uint8_t runSta;//
    uint8_t runAnswer; // 启动应答
    uint8_t devErrCode;//故障码
}TYPE_MSHGJ_DEV_STA;

extern void lyy_device_mshgj_init(void);
extern int mshgj_pay_start(uint16_t mode);
extern int mshgj_pay_apply(void);
extern void mshgj_packet_parse(uint8_t *package, uint16_t len);

#endif



