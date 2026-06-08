/*
 * @author: e.he
 * @Date: 2022-03-24 14:28:33
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-08-01 00:33:04
 * @Description: 
 */
#ifndef __LYY_START_H__
#define __LYY_START_H__


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


typedef struct {

    uint32_t    number;
    char        uniqueid[64]; 
    char        pay_apply_id[64];
    uint16_t    x;
    uint16_t    y;
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
extern int      api_std_start_entryQue(uint8_t type,uint16_t num,char *order , uint16_t x, uint16_t y, char *id);



#endif


#endif