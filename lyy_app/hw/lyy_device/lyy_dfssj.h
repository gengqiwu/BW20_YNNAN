/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-11 16:12:27
 * @Description: 
 */
#ifndef __LYY_DFSSJ_H__
#define __LYY_DFSSJ_H__

#include "stdint.h" 
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

#define DFSSJ_LANE_MAX 10 // 最大出水口数量
#define DFSSJ_STATE_STR_MAX_LEN (DFSSJ_LANE_MAX * 6 + 1) // 状态最大上报字符串长度

enum DFSSJ_SUBCMD_E
{
    DFSSJ_SUBCMD_NETSTATE = 0X01,  // 网络状态
    DFSSJ_SUBCMD_PAY_START,        // 支付启动
    DFSSJ_SUBCMD_START_END,        // 启动订单结束
    DFSSJ_SUBCMD_ONE_STATE,        // 查询一个出水口状态
    DFSSJ_SUBCMD_ALL_STATE,        // 查询所有出水口状态
    DFSSJ_SUBCMD_SET_PARAM,        // 模式之类的设置参数
    DFSSJ_SUBCMD_GET_PARAM,        // 模式之类的查询参数
    DFSSJ_SUBCMD_SWIPING_CARD,     // 刷卡卡号上报
    DFSSJ_SUBCMD_SWIPING_CARD_END, // 刷卡订单结束上报
    DFSSJ_SUBCMD_TIME,             // 查询时间（设备向盒子查询时）
    DFSSJ_SUBCMD_WATER_TEMP,       // 水质温度查询
    DFSSJ_SUBCMD_GET_CONSUMABLE,   // 耗材查询
    DFSSJ_SUBCMD_SET_CONSUMABLE,   // 耗材设置
    DFSSJ_SUBCMD_GET_IC,           // IC卡查询

    DFSSJ_SUBCMD_FAULT = 0x10, // 故障上报
    DFSSJ_SUBCMD_GET_PARAM_2,  // 温度 定制之类的参数 查询
    DFSSJ_SUBCMD_SET_PARAM_2,  // 温度 定制之类的参数 设置

    DFSSJ_SUBCMD_BIND_STATA = 0x19, // 解绑绑定
};

/**
 * @brief
 * 启动结果
*/
typedef struct
{
    uint8_t lane_id;    // 出水口ID
    uint8_t pack_id[6]; // 指令唯一码
} __attribute__((aligned(1), packed)) DFSSJ_START_RESULT_CMD_DEF, *PDFSSJ_START_RESULT_CMD_DEF;

typedef struct
{
    uint8_t flag;
    char order[128];
    DFSSJ_START_RESULT_CMD_DEF start_result_cmd;
} __attribute__((aligned(1), packed)) DFSSJ_START_INFOR_DEF, *PDFSSJ_START_INFOR_DEF;

/**
 * @brief
 * 德锋售水机-上报事件信息
 */
typedef struct
{
    DFSSJ_START_INFOR_DEF start[DFSSJ_LANE_MAX];
} __attribute__((aligned(1), packed)) DFSSJ_UPDATE_EVENT_DEF, *PDFSSJ_UPDATE_EVENT_DEF;

/**
 * @brief 启动参数
 *
 */
typedef struct
{
    uint8_t sub_cmd;         // 启动命令
    uint8_t pay_order[6];    // 支付唯一码
    uint8_t method;          // 出水方式
    uint8_t pay_amount[4];   // 支付金额
    uint8_t out_water[2];    //出水量或者时间
    uint8_t port;            //出水口

} __attribute__((aligned(1), packed)) DFSSJ_PAY_START_DEF,*PDFSSJ_PAY_START_DEF;

/**
 * @brief 出水口状态
 *
 */
typedef struct
{
    uint8_t state;                       // 0 空闲  1出水中 2无效 3故障
} __attribute__((aligned(1), packed)) DFSSJ_STATE_DEF;


/**
 * @brief  出水口数量，通道状态=等
 * 
 */
typedef struct
{
    uint8_t sum;                              // 套餐数量或者说出水口数量
    DFSSJ_STATE_DEF lane[DFSSJ_LANE_MAX];     //  几个出水口的状态
} __attribute__((aligned(1), packed)) DFSSJ_PARMA_DEF;

extern DFSSJ_PARMA_DEF dfssj_parma_t;

extern PDFSSJ_UPDATE_EVENT_DEF dfssj_get_update_event_infor(void);
extern void lyy_device_dfssj_task_init(void);
extern void device_dfssj_packet_parse(uint8_t *package, uint16_t len);
extern uint8_t device_dfssj_pay_apply(uint8_t id);
extern uint8_t deivce_dfssj_pay_start(uint8_t id, uint16_t number);
extern void device_dfssj_bind_state_info(uint8_t state);

#endif



