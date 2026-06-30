/*
 * @author: e.he
 * @Date: 2024-06-20 17:41:16
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-07-31 20:49:17
 * @Description: 
 */
#ifndef __MODEL_EVENT_H__ 
#define __MODEL_EVENT_H__


//账目类型
enum acc_type
{
    ACC_TYPE_ONLINE,          //线上账目
    ACC_TYPE_OFFLINE,         //线下账目
    ACC_TYPE_HW_MAX
};


#define ACC_PULSE_EVENT_ID_GIFT             "accounts_gift"
#define ACC_PULSE_EVENT_ID_COINS            "accounts_coin"
#define ACC_PULSE_EVENT_ID_OUT_COINS        "accounts_outCoin"

extern void event_post_query_pay_channels(void);
extern void event_post_query_business_infor(void);
extern void event_post_query_pay_packages(char *package_type);
extern void event_post_query_pay_packages_ndj(char *package_type,int x,int y,char *package_all);
extern void event_post_query_lane_info_post(void);
extern void event_post_account_ndj(uint16_t coin,uint32_t coins,uint16_t gift,uint32_t gifts,uint8_t lane_id,char *order);
extern void event_post_board_err_ndj(uint8_t error,uint8_t x,uint8_t y);
extern void event_post_start_result_ndj(uint16_t result,uint8_t x,uint8_t y,char *order);

extern void event_post_account_dbj_uart_change(char *type, uint32_t acc_sum, uint32_t acc_inc, char *unqueId);

extern void event_post_query_dynamic_url_ndj(char *package_id, char *channel, char *channel_number, uint32_t x, uint32_t y);
extern void event_post_query_dynamic_url_std(char *package_id,char *channel,char *channel_number);
extern void event_post_out_coin_result(char *result, uint32_t out_coins, char *unique_code,float pay_amount);
extern void event_post_account_dbj_uart(char *type,uint32_t acc_sum,uint32_t acc_inc,uint32_t out_sum,uint32_t out_inc,char *unqueId);
extern void event_post_account_wwj_uart(char *pay_method,uint32_t coin_sum,uint32_t coin_inc,uint32_t gift_sum,uint32_t gift_inc, char *order);
extern void event_post_account_pulse(uint32_t inc,uint32_t sum,char *pay_mode,char *order,char *event_id);
extern void event_post_account_amy_uart(char *pay_method,uint32_t coin_sum,uint32_t coin_inc,uint32_t cash_sum,uint32_t cash_inc, char *order);
extern void event_post_start_result_wwj(uint32_t gifts,uint32_t coins,uint32_t refund,char *order);
extern void event_post_err_message(char *language, int login);

extern void event_post_log(char *log);
extern void event_post_log_hextostring(uint8_t *data, uint16_t len);
extern void event_post_start_result_dfssj(uint16_t volume, uint8_t x, uint8_t y, char *order);

#endif // !
