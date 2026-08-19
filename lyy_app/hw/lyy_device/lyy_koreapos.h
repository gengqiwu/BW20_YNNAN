/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:12:27
 * @Description:
 */
#ifndef __LYY_KOREAPOS_H__
#define __LYY_KOREAPOS_H__

#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

typedef struct
{
    char transaction_number[50];  // 有应答的  系统交易流水号
    char transaction_number_new[50];  // 刚收到的新系统交易流水号
    uint32_t payment_amount;      //支付金额
    uint8_t pay_flag;             //支付状态  0 空闲  1 待支付  

} __attribute__((aligned(1), packed)) KOREAPOS_PARAM_DEF, *PKOREAPOS_PARAM_DEF;

extern KOREAPOS_PARAM_DEF koreapos_param_t;

extern void lyy_koreapos_pay_success_ACK(void);
extern void lyy_koreapos_payment(uint32_t amount, uint16_t timeout);
extern void lyy_koreapos_cancel_payment(void);
extern void koreapos_packet_parse(uint8_t *package, uint16_t len);


#endif
