/**
 * @file lyy_start.h
 * @brief 支付事件参数、申请及启动结果码和启动队列提交接口。
 */
/*
 * @author: e.he
 * @Date: 2022-03-24 14:28:33
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-08-01 00:33:04
 * @Description: 
 */
#ifndef __LYY_START_H__
#define __LYY_START_H__

#include <stdint.h>

enum
{
    START_TYPE_PAY_APPLY,
    START_TYPE_PAY_START,
    START_TYPE_COIN_IN,
    START_TYPE_GIFT_OUT,
    START_TYPE_NDJ_LANE_INFOR_GET,
    START_TYPE_COIN,
    START_TYPE_LEVEL,
};

/**
 * @brief 
 * 
 */
enum
{
    PAY_APPLY_RESULT_FAIL = 0,
    PAY_APPLY_RESULT_SUCCESS ,
    PAY_APPLY_RESULT_FAULT,
    PAY_APPLY_RESULT_OFFLINE,
    PAY_APPLY_RESULT_STARTING,
    PAY_APPLY_RESULT_TIMEOUT,
    PAY_APPLY_RESULT_OTHER,
};
/**
 * @brief 
 * 支付启动结果
 */
enum
{
    PAY_START_RESULT_FAIL = 0,
    PAY_START_RESULT_SUCCESS,
    PAY_START_RESULT_FAULT,
    PAY_START_RESULT_OFFLINE,
    PAY_START_RESULT_STARTING,
    PAY_START_RESULT_TIMEOUT,
    PAY_START_RESULT_SUCCESS_DBJ,
    PAY_START_RESULT_FAIL_DBJ,
    PAY_START_RESULT_OTHER,
};
/**
 * @brief 
 * 获取二维码参数
 */
typedef struct 
{
    uint8_t wallet_num;     // 钱包序号
    uint8_t package_id;     // 套餐ID
    char uinque[10][12];    //支付唯一码
}__attribute__ ((aligned (1), packed)) GET_PAY_URL_PARAM_DEF;


/**
 * @brief 事件队列复制的支付请求，channel 表示主板通道而非原货道二维坐标。
 * @note uniqueid 与 pay_apply_id 分别保存订单号和云端请求标识，提交入口均最多复制 63 字节。
 */
typedef struct {

    uint32_t    number;
    uint32_t channel;
    char        uniqueid[64]; 
    char        pay_apply_id[64];
}__attribute__((aligned(1), packed))HW_PAY_APPLY,*PHW_PAY_APPLY;


#if 0
extern void lyy_std_start_way(void *arg);
extern uint16_t pay_Repeat(uint8_t *pay_infor);
extern void     lyy_start_way(uint8_t *order,uint8_t len,uint16_t num, uint8_t type);

extern uint8_t pay_url_wallet_number_get(void);

extern int cmp_pay_url_uinque_code(uint8_t id, char *uinque_id);
extern char *get_py_url_uinque_code(uint8_t id);

#else

extern void     pay_url_wallet_number_set(uint8_t number);
extern int      cmp_pay_url_uinque_code(uint8_t id, char *uinque_id);
extern char     *get_py_url_uinque_code(uint8_t id);
extern uint8_t  pay_url_package_id_get(void);
extern void     pay_url_package_id_set(uint8_t package_id);
extern uint8_t  pay_url_wallet_number_get(void);
/**
 * @brief 尝试复制并提交支付申请或启动事件。
 * @param[in] type 仅 START_TYPE_PAY_APPLY、START_TYPE_PAY_START 触发提交。
 * @param[in] num 启动数量。
 * @param[in] order 订单号，可截断为 63 字节。
 * @param[in] x 主板通道。
 * @param[in] y 未使用的兼容参数。
 * @param[in] id 请求标识，可截断为 63 字节。
 * @retval 0 order 或 id 为空。
 * @retval 1 其余情况，不代表实际入队或硬件处理成功。
 */
extern int      api_std_start_entryQue(uint8_t type,uint16_t num,char *order , uint16_t x, uint16_t y, char *id);



#endif


#endif