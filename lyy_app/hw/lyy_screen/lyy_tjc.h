/**
 * @file lyy_tjc.h
 * @brief 淘晶驰屏幕页面状态、支付显示参数及主板透传收发接口。
 */
/*
 * @author: lyz
 * @Date: 2022-10-18 09:58:29
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-07 10:20:48
 * @Description: 
 */
#ifndef __LYY_TJC_H__
#define __LYY_TJC_H__

#include <stdint.h>

#include "HAL_System.h"
#include "FreeRTOS.h"
#include "task.h"

#define TJC_BUFF_MAX        512

#define HK_PAY_TYPE_WECHAT  0
#define HK_PAY_TYPE_ALIPAY  1

#define PAY_COUNTDOWN_MAX   40  // 40s
#define PAY_RESULT_MAX      5   //与安卓保持一致

#define   TJC_VERSION          "v3.0.1"
#define   THING_OTA_SCREEN     "screen"

//屏幕显示状态机
typedef enum
{
    TJC_SHOW_PACKAGES,        // 首页
    TJC_SHOW_UPDATE_NET,      // 空闲
    TJC_SHOW_COUNT_DOWN,      // 倒计时
    TJC_SHOW_PIC_SEND,        // 图片操作
    TJC_SHOW_WARING,          // 提示信息
    TJC_SHOW_DBJ_COINS_START, // 出币
    TJC_SHOW_DBJ_COINS,       // 出币
    TJC_SHOW_AMY_TIME_WORK,   // 倒计时
    TJC_SHOW_AMY_TIME_START,
    TJC_SHOW_AMY_PACKAGES,
    TJC_SHOW_LOG_DEBUG,  // 日志打印
    TJC_SHOW_PAY_URL,    // 支付界面
    TJC_SHOW_PAY_RESULT, // 正常支付成功
    TJC_SHOW_DEVICE_REGIST,
    TJC_SHOW_PAY_PAGE,            // 支付界面
    TJC_SHOW_STANDBY,             // 巡检状态
    TJC_SHOW_WARING_TO_PAY,       // 等待显示支付
    TJC_SHOW_WARING_TIP,          // 故障锁定显示
    TJC_SHOW_AMY_TIME_WORK_V2,    // 按摩椅故障界面
    TJC_SHOW_SCAN_POINTS,         //  扫描抵扣积分
    TJC_SHOW_SCAN_POINTS_SUCCESS, //  扫描抵扣积分,登录成功
    TJC_SHOW_EARN_POINTS,         //  获取积分页面
    TJC_SHOW_EARN_POINTS_SUCCESS, //  获取积分成功
    TJC_SHOW_PAY_RESULT_POINTS,   // 积分抵扣付款成功
    TJC_SHOW_SCREEN_OTA,   // 
} TJC_SHOW_STATE;

/**
 * @brief 保留的接收队列单元描述。
 * @note 当前接收入口直接解析而非入队，出队兼容接口固定返回 -1。
 */
typedef struct {
    uint16_t len;
    uint8_t *packet;
} __attribute__((aligned(1), packed)) STORAGE_QUE_DEF, *PSTORAGE_QUE_DEF;
/**
 * @brief 
 * 
 */


#define SCREEN_STATUS_NULL      0xff  //不需要切换状态
//屏幕显示状态机
typedef enum 
{
    PACKAGE_PAGE_NUMBER_1 = 0,      //1
    PACKAGE_PAGE_NUMBER_2,          //2
    PACKAGE_PAGE_NUMBER_3,          //3
    PACKAGE_PAGE_NUMBER_4,          //4
    PACKAGE_PAGE_NUMBER_5,          //5
}PACKAGE_PAGE_NUMBER;

#define TJC_SEND_PIC_PACKET_SIZE    2000//4096//1024*4

typedef struct
{
    int         code;           //保存机器故障信息
    uint32_t    time;           //倒计时时间
    uint8_t     next_status;    //倒计时到了之后的状态 
    char        context[254];   //显示屏状态  
}__attribute__((aligned(1), packed))TJC_PAGE_WARING_DEF,*PTJC_PAGE_WARING_DEF;

typedef struct
{
    uint32_t  time;             //倒计时时间
    uint8_t   next_status;      //倒计时到了之后的状态 
    uint8_t   enable;
}__attribute__((aligned(1), packed))TJC_COUNT_DOWN_DEF,*PTJC_COUNT_DOWN_DEF;
/**
 * @brief 
 * 
 */
typedef struct
{
    uint16_t    sum;    
    uint16_t    inc;
    uint16_t    time;
    uint8_t     flag;
}__attribute__((aligned(1), packed))TJC_DBJ_COINS_DEF,*PTJC_DBJ_COINS_DEF;
/**
 * @brief 
 * 
 */
typedef struct
{
    char        url[400];    //
    char        money[64];
    uint8_t     wallet_num;
    uint8_t     wallet_sum;
    uint16_t    time;
} __attribute__((aligned(1), packed)) TJC_PAY_PAGE_DEF, *PTJC_PAY_PAGE_DEF;
/**
 * @brief 
 * 成功页面
 */
typedef struct
{
    uint8_t     next_status;
    uint16_t    time;
}__attribute__((aligned(1), packed))TJC_PAY_SUCCESS_PAGE_DEF,*PTJC_PAY_SUCCESS_PAGE_DEF;

/**
 * @brief 
 * 成功页面
 */
typedef struct
{
    uint8_t     next_status;
    uint32_t    total;
    uint32_t    work_time;
}__attribute__((aligned(1), packed))TJC_AMY_TIME_PAGE_DEF,*PTJC_AMY_TIME_PAGE_DEF;
/**
 * @brief 
 * 页面倒计时
 */
typedef struct
{
    uint8_t             status;                 //显示屏状态
    char                tjc_ver[52];
    int                 fault_code;             //故障信息 
    uint8_t             img_flag;
    uint8_t             img_result;
    uint8_t             step;
    uint8_t             get_pay_channel_flag;   //获取了渠道信息
    uint8_t             get_regist_flag;        //获取注册信息
    TJC_COUNT_DOWN_DEF count_down;
    TJC_PAGE_WARING_DEF waring; 
    TJC_DBJ_COINS_DEF   dbj_coins;
    TJC_PAY_PAGE_DEF    pay;
    TJC_PAY_SUCCESS_PAGE_DEF success;
    TJC_AMY_TIME_PAGE_DEF amy_time;
}__attribute__((aligned(1), packed))TJC_INFOR_DEF,*PTJC_INFOR_DEF;

extern TJC_INFOR_DEF   tjc_infor_t;

/**
 * @brief 
 * 
 */
typedef struct
{
    uint64_t sys_tick_start;
    uint64_t sys_tick_last;
    uint64_t sys_tick_value;
    int32_t  work_value;
    int32_t  last_value;
}__attribute__((aligned(1), packed))AMY_WORK_TIME_DEF,*PAMY_WORK_TIME_DEF;
extern AMY_WORK_TIME_DEF  amy_time_t;

//显示屏触摸数据处理
typedef struct
{
    char    cmd[24];       //显示页
    void    (*parse)(char *pack,uint16_t len);    
}__attribute__((aligned(1), packed))TJC_PACKET_PARSE_DEF,*PTJC_PACKET_PARSE_DEF;


extern void     lyy_init_tjc(void);
extern void     lyy_tjc_showSets(void);
extern void     lyy_tjc_showPay(uint8_t picId);
extern void     lyy_tjc_changePayQRMoney(char *qr, char *money);

extern void     lyy_tjc_switch_page(char *page);
extern void     lyy_tjc_set_status(uint8_t newState);
extern void     picSend_startProcess(char *fileName, uint32_t fileSize);


extern void     lyy_tjc_device_infor(void);

extern void     lyy_tjc_ndj_clean_price(uint8_t id);
extern void     lyy_tjc_ndj_priceShow(uint8_t id,char *unit,float price,char *name);


extern void lyy_tjc_cmd_send(char *data, uint16_t len);
extern void tjc_packet_parse(uint8_t *packet, uint16_t len);
extern int tjc_packet_queEntry(uint8_t *pack, uint16_t len);
extern void     lyy_tjc_pay_page(PTJC_PAY_PAGE_DEF pay);
#if 0
extern void     lyy_tjc_pay_page_set(uint8_t wallet_num,uint8_t wallet_sum,char *url,char *money);
#endif
extern void     lyy_tjc_pay_page_set(uint8_t wallet_num,uint8_t wallet_sum);
extern void     lyy_tjc_pay_url_show(char *url,char *money);

extern void     lyy_tjc_page_package(void);
extern int      lyy_start_pic_get(void);
extern void     tjc_get_pay_channel_enable(void);
extern void     lyy_tjc_dbj_coins_set(uint16_t sum,uint16_t inc);

extern void     lyy_tjc_page_waring(char *context,uint16_t time,uint8_t next_status);
extern void     lyy_tjc_page_pay_reuslt(void);

extern void     lyy_tjc_page_regist_set(void);
extern void     lyy_tjc_page_fault_set(uint16_t code);

extern void     lyy_tjc_page_amy_time_set(uint32_t time);
extern int      amy_time_redisplay(void);


extern void     lyy_tjc_objTxt(char *objName, char *txt);
extern void     lyy_tjc_switch_page(char *page);
extern void     lyy_tjc_set_status(uint8_t status);
extern uint8_t  lyy_tjc_get_status(void);

extern void amy_time_to_string(uint32_t time,char *pstring);
extern void lyy_tjc_obj_update_amy_count_down(char *body);
extern void lyy_tjc_packages_choose(uint8_t id, uint8_t wallet_number);



#endif
