/**
 * @file thing_event.c
 * @brief 物模型业务事件封装，包含纸钞通道账目及最终启动结果上报。
 */
/*
 * @author: e.he
 * @Date: 2024-06-20 17:41:30
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 14:54:55
 * @Description: 
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "HAL_System.h"


#include "thing_iot.h"
#include "thing_init.h"
#include "thing_event.h"
#include "thing_service.h"
#include "thing_property.h"


#include "lyy_start.h"
#include "lyy_zcj.h"

#include "utils.h"
#include "cJSON.h"


/**
 * @description: 获取故障信息上报
 * @param {*}
 * @return {*}
 */
void event_post_err_message(char *language, int login)
{
    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddStringToObject(value, "language", language);
        cJSON_AddNumberToObject(value, "login_flag", login);
        thing_event_post(value, "query_customize_errors"); //内部删除json 
    }
}
/**
 * @description: 出币结果上报
 * @param {*}
 * result:0-启动正常 1-启动失败
 * @return {*}
 */
void event_post_out_coin_result(char *result, uint32_t out_coins, char *unique_code, float pay_amount)
{
    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddStringToObject(value, "result", result);
        cJSON_AddNumberToObject(value, "out_coins", out_coins);
        cJSON_AddStringToObject(value, "unique_code", unique_code);
        cJSON_AddNumberToObject(value, "pay_amount", pay_amount);
        thing_event_post(value, "out_coin_result"); //内部删除json 
    }
}

/**
 * @brief 更新纸钞通道累计值并上报账目事件。
 * @param[in] inc 本次通道增量。
 * @param[in] sum 该通道最新累计值。
 * @param[in] pay_mode 支付方式字符串。
 * @param[in] order 订单号；NULL 时生成随机字符串。
 * @param[in] event_id 云端事件标识。
 * @param[in] channel 主板通道；硬币脉冲 101 写入 ch_acc[0]，纸钞脉冲 100 写入 ch_acc[1]，两者上报通道均为 9；0～10 写入 ch_acc[1+channel]。
 * @note channel_count 仅拼接 ch_acc[2..9] 和 ch_acc[0]，不含 ch_acc[1]。
 * @warning 总账 ch_sum 为 0 时直接丢弃，不更新累计值也不补发；JSON 分配失败也不发送，不提供离线持久化。
 */
void event_post_account_note(uint32_t inc, uint32_t sum, char *pay_mode, char *order, char *event_id, int32_t channel)
{
    char sum_acc[128] = {0};
    ZCJ_NOTE_ACCS_TOTAL *acct = zcj_accounts_get();
    cJSON *value;
    uint8_t ch = channel;
    if (acct->ch_sum == 0) return;
    if (channel == 101) { ch = 9; acct->ch_acc[0] = sum; }
    else if (channel == 100) { ch = 9; acct->ch_acc[1] = sum; }
    else if (channel >= 0 && channel <= 10) acct->ch_acc[1 + channel] = sum;
    snprintf(sum_acc, sizeof(sum_acc), "%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu",
             (unsigned long)acct->ch_acc[2], (unsigned long)acct->ch_acc[3],
             (unsigned long)acct->ch_acc[4], (unsigned long)acct->ch_acc[5],
             (unsigned long)acct->ch_acc[6], (unsigned long)acct->ch_acc[7],
             (unsigned long)acct->ch_acc[8], (unsigned long)acct->ch_acc[9],
             (unsigned long)acct->ch_acc[0]);
    value = cJSON_CreateObject();
    if (value == NULL) return;
    cJSON_AddNumberToObject(value, "channel", ch);
    cJSON_AddNumberToObject(value, "channel_increment", inc);
    cJSON_AddStringToObject(value, "channel_count", sum_acc);
    cJSON_AddStringToObject(value, "pay_method", pay_mode);
    cJSON_AddStringToObject(value, "order", order == NULL ? get_random_string() : order);
    thing_event_post(value, event_id);
}
/**
 * @brief 将主板 0x07 最终执行结果封装为 start_result 事件。
 * @param[in] result 最终结果码，通过 get_apply_start_result_type 转为字符串。
 * @param[in] channel 主板返回通道，原值上报。
 * @param[in] order 对应订单号。
 * @note 区别于 pay_start 即时接单应答；JSON 分配失败时直接返回。
 */
void event_post_start_result_note(uint32_t result, uint32_t channel, char *order)
{
    cJSON *value = cJSON_CreateObject();
    if (value == NULL) return;
    cJSON_AddNumberToObject(value, "channel", channel);
    cJSON_AddStringToObject(value, "result", get_apply_start_result_type(result));
    cJSON_AddStringToObject(value, "order", order);
    thing_event_post(value, "start_result");
}
/**
 * @brief 
 * 脉冲版本账目
 * @param inc 
 * @param sum 
 * @param unqueId 
 */
void event_post_account_pulse(uint32_t inc,uint32_t sum,char *pay_mode,char *order,char *event_id)
{
    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "inc", inc);
        cJSON_AddNumberToObject(value, "sum", sum);
        cJSON_AddStringToObject(value, "pay_method", pay_mode);
        if(order == NULL)
        {
  
            cJSON_AddStringToObject(value, "order", get_random_string());
        }
        else
        {
            cJSON_AddStringToObject(value, "order", order);
        }
        thing_event_post(value, event_id); 
    }
}

/**
 * @brief 
 * 账目-兑币机
 * @param type  线上/线下
 * @param acc_sum  现金
 * @param acc_inc  现金
 * @param out_sum  出币
 * @param out_inc  出币
 * @param unqueId  指令唯一码
 */
void event_post_account_dbj_uart(char *type,uint32_t acc_sum,uint32_t acc_inc,uint32_t out_sum,uint32_t out_inc, char *unqueId)
{

    cJSON* value    = NULL;
    char id[6]      = {0x00,};
    char id_str[15] = {0x00,};

    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "cash_increment", acc_inc);
        cJSON_AddNumberToObject(value, "out_coins_increment", out_inc);
        cJSON_AddNumberToObject(value, "cash_total", acc_sum);
        cJSON_AddNumberToObject(value, "out_coins_total", out_sum);
        cJSON_AddStringToObject(value, "pay_method", type);
        if(unqueId == NULL)
        {
            //使用随机数生辰订单号
            vRandomGet(id, sizeof(id));
            utils_lhexToString(id_str, (const unsigned char *)id, sizeof(id));
            cJSON_AddStringToObject(value, "unique_code", id_str);
        }
        else
        {
            cJSON_AddStringToObject(value, "unique_code", unqueId);
        }
        
        thing_event_post(value, "accounts"); 
    }
}

/**
 * @brief
 * 账目-找零-兑币机
 * @param type
 * @param acc_sum  找零增量
 * @param acc_inc  找零总量
 * @param unqueId  指令唯一码
 */
void event_post_account_dbj_uart_change(char *type, uint32_t acc_sum, uint32_t acc_inc, char *unqueId)
{
    cJSON *value = NULL;
    // char *pcData = NULL;
    char id[6] = {
        0x00,
    };
    char id_str[15] = {
        0x00,
    };

    value = cJSON_CreateObject();
    if (value != NULL)
    {
        cJSON_AddNumberToObject(value, "change_increment", acc_inc);
        cJSON_AddNumberToObject(value, "change_total", acc_sum);
        cJSON_AddStringToObject(value, "pay_method", type);
        if (unqueId == NULL)
        {
            // 使用随机数生辰订单号
            vRandomGet(id, sizeof(id));
            utils_lhexToString(id_str, (const unsigned char *)id, sizeof(id));
            cJSON_AddStringToObject(value, "unique_code", id_str);
        }
        else
        {
            cJSON_AddStringToObject(value, "unique_code", unqueId);
        }

        thing_event_post(value, "change_accounts");
    }
}

/**
 * @brief 
 * 账目-娃娃机
 * @param pay_method 
 * @param coin_sum 
 * @param coin_inc 
 * @param gift_sum 
 * @param gift_inc 
 * @param unqueId 
 */
void event_post_account_wwj_uart(char *pay_method,uint32_t coin_sum,uint32_t coin_inc,uint32_t gift_sum,uint32_t gift_inc, char *order)
{
    cJSON* value = NULL;
    // char *pcData = NULL;
    char id[6] = {0x00,};
    char id_str[15] = {0x00,};

    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "coin_sum", coin_sum);
        cJSON_AddNumberToObject(value, "coin_inc", coin_inc);
        cJSON_AddNumberToObject(value, "gift_sum", gift_sum);
        cJSON_AddNumberToObject(value, "gift_inc", gift_inc);
        cJSON_AddStringToObject(value, "pay_method", pay_method);
        if(order == NULL)
        {
            //使用随机数生辰订单号
            vRandomGet(id, sizeof(id));
            utils_lhexToString(id_str, (const unsigned char *)id, sizeof(id));
            cJSON_AddStringToObject(value, "order", id_str);
        }
        else
        {
            cJSON_AddStringToObject(value, "order", order);
        }
        
        thing_event_post(value, "accounts"); 
    }
}
/**
 * @brief 
 * 账目-按摩椅
 * @param pay_method 
 * @param coin_sum 
 * @param coin_inc 
 * @param cash_sum 
 * @param cash_inc 
 * @param unqueId 
 */
void event_post_account_amy_uart(char *pay_method,uint32_t coin_sum,uint32_t coin_inc,uint32_t cash_sum,uint32_t cash_inc, char *order)
{

    cJSON* value = NULL;
    char id[6] = {0x00,};
    char id_str[15] = {0x00,};

    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "coin_sum", coin_sum);
        cJSON_AddNumberToObject(value, "coin_inc", coin_inc);
        cJSON_AddNumberToObject(value, "cash_total", cash_sum);
        cJSON_AddNumberToObject(value, "cash_increment", cash_inc);
        cJSON_AddStringToObject(value, "pay_method", pay_method);
        if(order == NULL)
        {
            //使用随机数生辰订单号
            vRandomGet(id, sizeof(id));
            utils_lhexToString(id_str, (const unsigned char *)id, sizeof(id));
            cJSON_AddStringToObject(value, "order", id_str);
        }
        else
        {
            cJSON_AddStringToObject(value, "order", order);
        }
        
        thing_event_post(value, "accounts"); 
    }
}

/**
 * @description: 主板连接状态上报
 * @param {*}
 * status:0-offline 1-online
 * @return {*}
 */
void event_post_device_cnnt(uint8_t status)//
{

    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddStringToObject(value, "status", ((status==0)?"offline":"online"));
        thing_event_post(value, "connection_status"); 
    }

}


/**
 * @brief 
 * 获取动态支付二维码 -- 通用
 * @param package_id 
 * @param channel 
 * @param channel_number 
 * @param x 
 * @param y 
 */
void event_post_query_dynamic_url_std(char *package_id,char *channel,char *channel_number)
{

    cJSON* value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddStringToObject(value, "commodity_id",package_id);//套餐id
        cJSON_AddStringToObject(value, "pay_channel_code",channel);//支付渠道
        cJSON_AddStringToObject(value, "pay_channel_number",channel_number);//渠道编号
        cJSON_AddStringToObject(value, "unique_code",get_py_url_uinque_code(0));//支付唯一码
        thing_event_post(value, "get_qrcode");
    }

}

/**
 * @brief
 * 获取动态支付二维码 --扭蛋机
 * @param package_id
 * @param channel
 * @param channel_number
 * @param x
 * @param y
 */
void event_post_query_dynamic_url_ndj(char *package_id, char *channel, char *channel_number, uint32_t x, uint32_t y)
{

    cJSON *value = cJSON_CreateObject();
    if (value != NULL)
    {
        cJSON_AddStringToObject(value, "commodity_id", package_id);               // 套餐id
        cJSON_AddStringToObject(value, "pay_channel_code", channel);              // 支付渠道
        cJSON_AddStringToObject(value, "pay_channel_number", channel_number);     // 渠道编号
        cJSON_AddStringToObject(value, "unique_code", get_py_url_uinque_code(x)); // 支付唯一码
        cJSON_AddNumberToObject(value, "position_x", x);                          //
        cJSON_AddNumberToObject(value, "position_y", y);                          //
        thing_event_post(value, "get_qrcode");
    }
}

/**
 * @brief 
 * 支付套餐查询
 * @param package_type-"ALL"充值&消费 "RECHARGE"充值 "CONSUMPTION"消费
 * @param x 
 * @param y 
 * @param package_all  true ,false
 */
void event_post_query_pay_packages_ndj(char *package_type,int x,int y,char *package_all)
{
    cJSON* value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddStringToObject(value, "package_type", package_type);   //套餐类型 
        cJSON_AddStringToObject(value, "all_package", package_all);     //是否获取全部套餐
        cJSON_AddNumberToObject(value,"position_x",x);
        cJSON_AddNumberToObject(value,"position_y",y);
        thing_event_post(value, "query_position_pay_package");
    }
}

/**
 * @brief 
 * 获取支付套餐
 * @param package_type 
 */
void event_post_query_pay_packages(char *package_type)
{
    cJSON* value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddStringToObject(value, "package_type", package_type);   //套餐类型 
        thing_event_post(value, "query_pay_package");
    }
}

/**
 * @brief 
 * 支付渠道查询
 * @param 
 */
void event_post_query_pay_channels(void)
{
    cJSON* value = cJSON_CreateObject();
    if(value != NULL)
    {
        //支付调起方式
        cJSON_AddStringToObject(value, "presentation_method", "QRCODE"); 
        thing_event_post(value, "query_pay_channel");
    }
}

/**
 * @brief 
 * 获取支付二维码
 * @param type 
 */
void event_post_query_pay_qr_code(uint8_t type)
{
    cJSON* value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "type", type);
        thing_event_post(value, "query_pay_qr_code");
    }
}

/**
 * @brief 
 * 获取语言区域等参数
 * @param 
 */
void event_post_query_business_infor(void)
{
    cJSON* value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddStringToObject(value, "param", "1");//套餐类型 
        thing_event_post(value, "query_equipment_info");
    }
}

/**
 * @brief 
 * 获取货道信息
 * @param 
 */
void event_post_query_lane_info_post(void)
{
    cJSON* value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddStringToObject(value, "0", "0");//套餐类型 
        thing_event_post(value, "query_position_info");
    }
}

/**
 * @brief 
 * 
 * @param gifts 
 * @param coins 
 * @param refund 
 * @param order 
 */
void event_post_start_result_wwj(uint32_t gifts,uint32_t coins,uint32_t refund,char *order)
{
    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "coin",coins);
        cJSON_AddNumberToObject(value, "gift", gifts);
        cJSON_AddNumberToObject(value, "refund", refund);
        cJSON_AddStringToObject(value, "order", order);
        thing_event_post(value, "start_result"); 
    }
}

/**
 * @brief 
 * 账目--ndj
 * @param coin 
 * @param coins 
 * @param gift 
 * @param gifts 
 * @param lane_id 
 * @param order 
 */
void event_post_account_ndj(uint16_t coin,uint32_t coins,uint16_t gift,uint32_t gifts,uint8_t lane_id,char *order)
{
    cJSON* value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "coin_inc", coin);//投币增量
        cJSON_AddNumberToObject(value, "gift_inc", gift);//出礼增量
        cJSON_AddNumberToObject(value, "coin_sum", coins);//投币总数
        cJSON_AddNumberToObject(value, "gift_sum", gifts);//出礼总数
        cJSON_AddNumberToObject(value,"position_x",lane_id);//id
        cJSON_AddNumberToObject(value,"position_y",0);//id
        cJSON_AddStringToObject(value, "order", order);//订单号
        cJSON_AddStringToObject(value, "pay_method", "offline_pay");//方式
        thing_event_post(value, "accounts");
    }
}

/**
 * @brief 
 * 主板故障上报
 * @param error 
 */
void event_post_board_err_ndj(uint8_t error,uint8_t x,uint8_t y)
{
    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "error", error);
        cJSON_AddNumberToObject(value, "position_x", x);
        cJSON_AddNumberToObject(value, "position_y", y);
        thing_event_post(value, "board_error"); 
    }
}

/**
 * @brief 
 * 主板故障上报--娃娃机
 * @param error 
 * @param x 
 * @param y 
 */
void event_post_board_err_wwj(uint8_t error,uint8_t x,uint8_t y)
{
    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "error", error);
        cJSON_AddNumberToObject(value, "position_x", x);
        cJSON_AddNumberToObject(value, "position_y", y);
        thing_event_post(value, "board_error"); 
    }
}

/**
 * @brief 
 * 启动结果上报
 * @param result 
 * @param x 
 * @param y 
 * @param order 
 */
void event_post_start_result_ndj(uint16_t result,uint8_t x,uint8_t y,char *order)
{
    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "coins", result);
        cJSON_AddNumberToObject(value, "position_x", x);
        cJSON_AddNumberToObject(value, "position_y", y);
        cJSON_AddStringToObject(value, "order", order);
        thing_event_post(value, "start_result"); 
    }
}

/**
 * @brief 
 * 
 * @param error 主板故障上报
 */
void event_post_device_fault(uint8_t error)
{
    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddNumberToObject(value, "error", error);
        thing_event_post(value, "board_error"); 
    }
}
