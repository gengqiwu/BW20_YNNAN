/*
 * @author: e.he
 * @Date: 2024-07-10 16:31:10
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-29 16:41:09
 * @Description: 
 */
#ifndef __LYY_CZF_H__
#define __LYY_CZF_H__

#include <stdint.h>

// 正向主板协议+3 +4
typedef struct
{
    uint8_t head;
    uint8_t len[2];
    uint8_t cmd;
    uint8_t sud_cmd;
    uint8_t data[1255];
    uint8_t check;
    uint8_t end;
} __attribute__((aligned(1), packed)) DEVICE_PACK_CZF_DEF, *PDEVICE_PACK_CZF_DEF;

// 主板带价格支付二维码请求
typedef struct
{
    uint8_t sud_cmd;
    uint8_t amount[4];
    uint8_t channelNumber[64];
    uint8_t wallet[32];
    uint8_t unique[6];
} __attribute__((aligned(1), packed)) DEVICE_GET_QRCODE_CZF_DEF, *PDEVICE_GET_QRCODE_CZF_DEF;

// 支付渠道
typedef struct
{
    uint8_t number;              //渠道序号
    char    channel_number[64]; // 渠道编号
    char    code[32];              // 钱包编码
    char    name[32];                 // 钱包名称
    char    pay_type[32];         // 钱包类型
} __attribute__((aligned(1), packed)) DEVICE_PAY_CHANNEL_CZF_DEF, *PDEVICE_PAY_CHANNEL_CZF_DEF;

extern void device_czf_packet_parse(uint8_t subCmd, uint8_t *data, uint16_t len);
extern void device_packet_czf_data_length(uint8_t sud_cmd, uint8_t *data, uint16_t dataLen);
#endif
