/*
 * @author: e.he
 * @Date: 2024-07-02 16:12:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-26 16:32:58
 * @Description: 
 */
#ifndef __LYY_NDJ_H__
#define __LYY_NDJ_H__

#include "lyy_para.h"
#include "lyy_device.h"

/**
 * @brief 
 * 缓存信息
 */
typedef struct
{
    char get_lane_id[64];
    char flag_set_lane;
    char flag_get_lane;
    char set_lane_id[64];
    CMD_ID_U start_cmd_id;
    uint8_t dzy_flag;
    uint8_t lane_id;
}__attribute__((aligned(1), packed))NDJ_INFOR,*PNDJ_INFOR;
extern NDJ_INFOR ndj_infor_t;
/**
 * @brief 
 * 渠道信息
 */
typedef struct
{
    uint8_t         sum;
    uint8_t         number;             //
    char            wallet[64];         //钱包
    char            name[64];           //名称
    char            icon[128];          //图标
    char            id[64];             //编号
    char            channel[64];        //渠道编码
    char            bg_code[64];        //背景
    char            bg_url[128];        //背景地址
    char            region[64];         //地区
}__attribute__((aligned(1), packed))NDJ_CHANNEL_INFOR,*PNDJ_CHANNEL_INFOR;
/**
 * @brief 
 * 货道信息
 */
typedef struct
{
    uint8_t         sum;
    uint16_t        play_time;         //游戏时间 秒
    uint16_t        status;            //状态 0 在线 1离线
    uint16_t        startup_unit;      //几币一局
    uint16_t        enable;            //是否启用

    //增加礼品名称
    uint64_t        giftId;             //礼品Id
    char            giftName[64];       //礼品名称 
}__attribute__((aligned(1), packed))NDJ_LANE_DEF,*PNDJ_LANE_DEF;
/**
 * @brief 
 * 
 */
typedef struct
{
    uint8_t         sum;
    char            id[32];             //套餐id
    char            sku[64];            //sku编码
    char            name[128];          //名称
    uint32_t        seq;                //排序
    float           price;              //套餐金额
    char            display_price[32];  //展示金额
    float           priceValue;         //套餐币数  
    char            display_value[32];  //展示价值
    char            unit[32];           //套餐币单位
    char            translate[32];      //翻译 
}__attribute__((aligned(1), packed))NDJ_PACKAGE_DEF,*PNDJ_PACKAGE_DEF;
/**
 * @brief 
 * 盒子信息
 */
typedef struct
{
    char            soft_ver[64];        //
    char            hard_ver[64];        //
    char            dev_id[64];          //
    uint8_t         net_status;          //
    uint8_t         csq;                 //
}__attribute__((aligned(1), packed))NDJ_DZY_BOX_INFOR_DEF,*PNDJ_DZY_BOX_INFOR_DEF;


/**
 * @brief 
 * 扭蛋机-支付申请
 */
typedef struct
{
    uint8_t    sub_id;          //自设备id
    uint8_t    sub_cmd;         //子命令
    uint8_t    id;              //仓位id
    uint8_t    pay_order[6];    //支付唯一码
}__attribute__((aligned(1), packed))NDJ_PAY_APPLY_DEF,*PNDJ_PAY_APPLY_DEF;
/**
 * @brief 
 * 扭蛋机-支付
 */
typedef struct
{
    uint8_t    sub_id;          //自设备id
    uint8_t    sub_cmd;         //子命令
    uint8_t    id;              //仓位id
    uint8_t    pay_order[6];    //支付唯一码
    uint16_t   number;          //支付数量
}__attribute__((aligned(1), packed))NDJ_PAY_DEF,*PNDJ_PAY_DEF;
/**
 * @brief 
 * 查询指令
 */
typedef struct
{
    uint8_t    sub_id;          //
    uint8_t    sub_cmd;          //
    uint8_t    lane_id;          //仓位ID
}__attribute__((aligned(1), packed))NDJ_GET_CMD_DEF,*PNDJ_GET_CMD_DEF;
/**
 * @brief 
 * 扭蛋机-单个仓位信息上报
 */
typedef struct
{
    uint8_t    sub_id;      //子设备id
    uint8_t    sub_cmd;     //子命令
    uint8_t    id;          //仓位ID
    uint8_t    status;      //状态
    uint8_t    coins;       //几币一局
    uint8_t    time;        //时间
}__attribute__((aligned(1), packed))LANE_FORMAT_DEF,*PLANE_FORMAT_DEF;

/**
 * @brief 
 * 扭蛋机-开机仓位信息上报
 */
typedef struct
{
    uint8_t         sub_id;                     //
    uint8_t         sub_cmd;                    //仓位ID
    uint8_t         ver_soft[14];               //软件版本
    uint8_t         number;                     //仓位数量
    LANE_FORMAT_DEF infor[NDJ_LANE_MAX];        //仓位信息
}__attribute__((aligned(1), packed))NDJ_LANE_PACKET_FORMAT_DEF,*PNDJ_LANE_PACKET_FORMAT_DEF;

/**
 * @brief 
 * 账目指令
 */
typedef struct
{
    uint8_t         flag;
    uint8_t         sub_id;             //
    uint8_t         sub_cmd;            // 子命令
    uint8_t         lane_id;            //仓位ID
    uint8_t         pack_id[6];         //指令唯一码
    uint16_t        coin_inc;           //投币
    uint32_t        coin_sum;           //
    uint16_t        gift_inc;           //出礼
    uint32_t        gift_sum;           //
}__attribute__((aligned(1), packed))NDJ_ACC_PACKET_FORMAT_DEF,*PNDJ_ACC_PACKET_FORMAT_DEF;
/**
 * @brief 
 * 故障
 */
typedef struct
{
    uint8_t         flag;
    uint8_t         sub_id;             //
    uint8_t         sub_cmd;            // 子命令
    uint8_t         lane_id;            //仓位ID
    uint8_t         pack_id[6];        //指令唯一码
    uint8_t         fault;              //故障
}__attribute__((aligned(1), packed))NDJ_FAULT_PACKET_FORMAT_DEF,*PNDJ_FAULT_PACKET_FORMAT_DEF;

/**
 * @brief 
 * 启动结果
 */
typedef struct
{
    uint8_t         sub_id;             //
    uint8_t         sub_cmd;            // 子命令
    uint8_t         lane_id;            //仓位ID
    uint8_t         pack_id[6];         //指令唯一码
    uint16_t        coins;              //上分数量
}__attribute__((aligned(1), packed))NDJ_START_RESULT_CMD_DEF,*PNDJ_START_RESULT_CMD_DEF;

typedef struct
{
    uint8_t         flag;
    char            order[128];
    NDJ_START_RESULT_CMD_DEF start_result_cmd;
}__attribute__((aligned(1), packed))NDJ_START_INFOR_DEF,*PNDJ_START_INFOR_DEF;

/**
 * @brief 
 * 扭蛋机-上报事件信息
 */
typedef struct
{
    NDJ_LANE_PACKET_FORMAT_DEF      lane;
    NDJ_ACC_PACKET_FORMAT_DEF       acc;
    NDJ_FAULT_PACKET_FORMAT_DEF     fault;
    NDJ_START_INFOR_DEF             start[NDJ_LANE_MAX];
}__attribute__((aligned(1), packed))NDJ_UPDATE_EVENT_DEF,*PNDJ_UPDATE_EVENT_DEF;


extern PNDJ_UPDATE_EVENT_DEF get_update_event_infor(void);
extern void ndj_dzy_get_qrcode_ack(uint8_t id,char *url);
extern void lyy_device_ndj_init(void);
extern void device_ndj_dzy_packet_parse(uint8_t *packet,uint16_t len);
extern void device_ndj_packet_parse(uint8_t *packet,uint16_t len);
extern void ndj_value_init(void);

extern uint8_t device_ndj_pay_apply(uint8_t sub_id,uint8_t id);
extern uint8_t deivce_ndj_pay_start(uint8_t sub_id,uint8_t id,uint16_t number);
extern uint8_t deivce_ndj_lane_infor_get(uint8_t sub_id,uint8_t id);

extern void set_channel_sum(uint8_t sum);

#endif


