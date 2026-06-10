/*
 * @author: e.he
 * @Date: 2024-07-09 18:38:39
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-08-09 11:29:10
 * @Description: 
 */

#ifndef __LYY_DBJ_H__
#define __LYY_DBJ_H__

#include <stdint.h>

//对应d1 03 出币上报指令
typedef struct
{
    uint8_t     sub_cmd;
    uint8_t     trade[6];           //订单唯一码
    uint16_t    pay;                //支付金额
    uint8_t     result;             //结果
    uint16_t    export_coin;        //纸币
}__attribute__((aligned(1), packed))DBJ_EXPORT_COIN_T,*PDBJ_EXPORT_COIN_T;
/**
 * @brief 
 * 出币上报---20
 */
typedef struct
{
    uint16_t    mohey;              //支付金额
    uint16_t    total;              //总数
    uint16_t    inc;                //增量
}__attribute__((aligned(1), packed))DBJ_EXPORT_COIN_20_T,*PDBJ_EXPORT_COIN_20_T;
/**
 * @brief 
 * 出币上报---D120
 */
typedef struct
{
    uint32_t    mohey;              //支付金额
    uint16_t    total;              //总数
    uint16_t    inc;                //增量
}__attribute__((aligned(1), packed))DBJ_EXPORT_COIN_D120_T,*PDBJ_EXPORT_COIN_D120_T;
//兑币机主动上传账目
typedef struct
{
    uint8_t     mark;       //查询主板链接类型
    uint8_t     fault;      //上分标识
    uint16_t    note;
    uint16_t    out_coin;
    uint16_t    free_coin;
    uint32_t    res;
}__attribute__((aligned(1), packed))DEVICE_ACC_DBJ_DEF,*PDEVICE_ACC_DBJ_DEF;
/**
 * @brief 
 * 线下账户---兑币机
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     unique[6];
    uint16_t    coins_inc;              //出币增量
    uint32_t    coins_sum;              //出币总量
    uint8_t     type;                   //类型 00 纸钞 01 硬币
    uint32_t    money_inc;              //现金增量
    uint32_t    money_sum;              //现金总量
}__attribute__((aligned(1), packed))DBJ_ACCOUNTS_DEF,*PDBJ_ACCOUNTS_DEF;


/**
 * @brief 
 * 海外兑币机-支付申请
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     unique[6];
}__attribute__((aligned(1), packed))HW_DBJ_PAY_APPLY_DEF,*PHW_DBJ_PAY_APPLY_DEF;

/**
 * @brief 
 * 海外兑币机-支付申请-应答
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     unique[6];
    uint8_t     status;//0x00-离线 0x01-待支付
}__attribute__((aligned(1), packed))HW_DBJ_PAY_APPLY_REPLY_DEF,*PHW_DBJ_PAY_APPLY_REPLY_DEF;


/**
 * @brief 
 * 出币启动 ---兑币机
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     unique[6];
    uint16_t    payAmount;      //支付金额
    uint16_t    coins;          //出币数
}__attribute__((aligned(1), packed))HW_DBJ_OUT_COINS_DEF,*PHW_DBJ_OUT_COINS_DEF;
/**
 * @brief 
 * 出币应答----兑币机
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     unique[6];
    uint16_t    payAmount;
    uint8_t     result;
    uint16_t    outCoins;
}__attribute__((aligned(1), packed))HW_DBJ_OUT_COINS_REPLY_DEF,*PHW_DBJ_OUT_COINS_REPLY_DEF;

/**
 * @brief 
 * 出币结果上报---兑币机-05
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     unique[6];
    uint16_t    payAmount;
    uint8_t     result;
    uint16_t    coins;
    uint32_t    total_coins;
}__attribute__((aligned(1), packed))DBJ_OUT_COINS_RESULT_DEF,*PDBJ_OUT_COINS_RESULT_DEF;


/**
 * @brief 
 * 出币结果上报---兑币机-03
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     unique[6];
    uint16_t    payAmount;
    uint8_t     result;
    uint16_t    coins;
}__attribute__((aligned(1), packed))DBJ_OUT_COINS_RESULT03_DEF,*PDBJ_OUT_COINS_RESULT03_DEF;

/**
 * @brief 
 * 出币结果应答--兑币机
 */
typedef struct
{
    uint8_t     sud_cmd;
    uint8_t     unique[6];
    uint8_t     result;
}__attribute__((aligned(1), packed))DBJ_OUT_COINS_RESULT_REPLY_DEF,*PDBJ_OUT_COINS_RESULT_REPLY_DEF;


extern void device_dbj_out_coins_cmd_20(uint8_t *pack,uint16_t len);
extern void device_dbj_packet_parse(uint8_t subCmd, uint8_t *data, uint16_t len);
extern void device_dbj_accounts_update(uint8_t *package, uint16_t len);

//海外
extern int  device_dbj_pay_apply(void);
extern int  device_dbj_pay_start(uint16_t payMount, uint16_t coins);

#endif



