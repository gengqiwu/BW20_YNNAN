/*
 * @author: e.he
 * @Date: 2022-02-17 10:59:30
 * @Description: 
 */
#ifndef __HAL_PARA_H__
#define __HAL_PARA_H__

#include <stdint.h>
#include "HAL_System.h"
#define FILE_NAME_PARA_DEVICE "para_box"
#define FILE_NAME_PARA_DEV_INFOR "para_dev_infor"
#define DEVICE_TYPE_UART_ZCJ 1
#define NET_TYPE_DEFAULT NET_TYPE_NETTY
typedef enum { NET_TYPE_LMQ, NET_TYPE_NETTY, NET_TYPE_MAX } ENUM_NET_TYPE;
//#define LYY_PATA_NDJ

//设备类型
typedef enum
{
    DEVICE_TYPE_DEFAULT,
    DEVICE_TYPE_PULSE,
    DEVICE_TYPE_LEVEL,
    DEVICE_TYPE_UART_CUSTOM,
    DEVICE_TYPE_UART_WWJ,
    DEVICE_TYPE_UART_DBJ,
    DEVICE_TYPE_UART_NDJ,
    DEVICE_TYPE_UART_AMY,
    DEVICE_TYPE_UART_XYJ,
    DEVICE_TYPE_UART_NOTE,
} DEVICE_TYPE;

typedef enum
{
    PARA_DEV,
    PARA_ACC,
    PARA_UPGRADE,
    PARA_MAX
}PARA_TYPE;



//
enum
{
    SERVER_ADDR_INFOR_MQTT_MOT_DEBUG,       //测试环境 
    SERVER_ADDR_INFOR_MQTT_MOT_DEVELOP,     //海外开发环境
    SERVER_ADDR_INFOR_MQTT_MOT_SG_RELEASE,  //新加坡正式环境

    SERVER_ADDR_INFOR_MAX,
};


enum{
    TYPE_2G  = 0x01,
    TYPE_4G,
    TYPE_WIFI,
};




enum LBS_COMPANY_TYPE
{
    LBS_COMPANY_TYPE_GOOGLE = 5,
    LBS_COMPANY_TYPE_AMAP = 6,
};

#define FILE_NAME_ERR_MSG          "lyy_err_msg"       //海外故障信息

#define NDJ_LANE_MAX 6
#define PAY_CHANNEL_MAX      4      //渠道数

/**
 * @brief 
 * 套餐参数
 */
typedef struct 
{
    char            id[32];             //套餐id
    uint32_t        seq;                //排序
    float           price;              //套餐金额
    char            display_price[32];  //展示金额
    float           priceValue;         //套餐币数  
    char            display_value[32];  //展示价值
    char            unit[32];           //套餐币单位
#if 0
    char            name[128];          //名称
    char            sku[64];            //sku编码   （？）
    char            translate[32];      //翻译   （？）
#endif
}__attribute__ ((packed, aligned(1))) PAY_PACKAGE_DEF;

/**
 * @brief 
 * 套餐
 */
typedef struct 
{
    uint8_t  number;  //套餐个数
    uint32_t x;
    uint32_t y;
    PAY_PACKAGE_DEF  pkg;
}__attribute__ ((aligned (1), packed)) NDJ_PACKAGE_LANE_DEF;

/**
 * @brief 
 * 设备套餐
 */
typedef struct 
{
    uint8_t              lane_sum;                          //套餐数量
    char                 currency_symbol[64];               //符号
    char                 currency_name[64];                 //名称
    NDJ_PACKAGE_LANE_DEF lane[NDJ_LANE_MAX];                //货到套餐
}__attribute__ ((aligned (1), packed)) PAY_PACKAGES_NDJ_DEF;


/**
 * @brief 
 * 设备套餐
 */
typedef struct 
{
    uint8_t         sum;                    //套餐数量
    char            currency_symbol[64];    //符号
    char            currency_name[64];      //名称
    PAY_PACKAGE_DEF pkg[NDJ_LANE_MAX];      //货到套餐
}__attribute__ ((aligned (1), packed)) PAY_PACKAGES_DEF;

/**
 * @brief 
 * 支付渠道信息
 */
typedef struct 
{
    char    code[64];          //钱包编码
    char    channel_number[64];//渠道编号
    char    channel_code[64];  //渠道编码
    char    name[64];          //钱包名称
    //no user
#if 0
    char    icon[128];         //钱包图标
    char    bg_code[64];       //背景编码
    char    bg_url[128];       //背景地址
#endif
    char    region[64];        //地区
}__attribute__ ((aligned (1), packed)) PAY_CHANNEL_DEF;

//业务信息
typedef struct 
{


#if 0
    char dev_id[64];        //业务id
    char region[64];        //修改成用来保存区域，根域区域来判断支付页面，小屏暂不支持文字语言切换
    char language[64];      //语音，暂无用
#endif
    uint8_t bind_state;     //绑定状态
}__attribute__ ((aligned (1), packed)) INFOR_BUSINESS;

//支付参数
typedef struct 
{
    char                    init_mark;                      //初始化标志位
    INFOR_BUSINESS          business;                       //业务设备信息
#ifdef LYY_PATA_NDJ
    PAY_PACKAGES_NDJ_DEF    ndj_pkgs;                       //ndj 套餐信息
#endif
    PAY_PACKAGES_DEF        pkgs;                           //其他品类 套餐个数
    uint8_t                 chnl_sum;                       //渠道数
    PAY_CHANNEL_DEF         channel[PAY_CHANNEL_MAX];       //支付渠道信息
}__attribute__ ((aligned (1), packed)) PAY_PARAM_DEF;


// 扭蛋机货道信息
typedef struct 
{
    uint16_t         position_x;        //
    uint16_t         position_y;        //
    uint16_t         play_time;         //游戏时间 秒
    uint16_t         status;            //状态 0 在线 1离线
    uint16_t         startup_unit;      //几币一局
    uint16_t         enable;            //是否启用

    //增加礼品名称
    uint64_t        giftId;             //礼品Id
    char            giftName[64];       //礼品名称
}__attribute__ ((aligned (1), packed)) LANE_DEF,*PLANE_DEF;

//扭蛋机货道表信息
typedef struct 
{
    uint8_t  number;
    uint8_t  flag[NDJ_LANE_MAX];
    LANE_DEF lane[NDJ_LANE_MAX];
}__attribute__ ((aligned (1), packed)) LANE_INFOR_TABLE_DEF,*PLANE_INFOR_TABLE_DEF;


//设备类型
typedef struct 
{
    char       name[32];   //设备类型 
}__attribute__ ((aligned (1), packed)) DEV_TYPE_DEF,*PDEV_TYPE_DEF;

typedef struct {
    uint8_t key[16], password[8], flag_aesUpdate;
} __attribute__((aligned(1), packed)) PARAAES_DEF, *PPARAAES_DEF;
typedef struct {
    uint16_t width, gap, mode;
    uint8_t note_type, type;
    uint32_t brt;
} __attribute__((aligned(1), packed)) PARAINTER_DEF, *PPARAINTER_DEF;
typedef struct {
    uint8_t type;
    uint16_t port;
    char url[100];
} __attribute__((aligned(1), packed)) PARAGATE_DEF, *PPARAGATE_DEF;
typedef struct {
    uint8_t init_mark, sw, mode;
    uint16_t cycle, number;
} __attribute__((aligned(1), packed)) PARA_FLOW_DEF, *PPARA_FLOW_DEF;
typedef struct {
    uint8_t init_mark;
    char id[33];
    uint32_t type;
    PARAAES_DEF aes;
    PARAINTER_DEF inter;
    PARAGATE_DEF gate;
    PARA_FLOW_DEF flow;
    uint16_t login_flag;
    uint8_t flag_sdk_upgrade, output_type, output_port, res[17];
    char dev_type[64];
} __attribute__((aligned(1), packed)) PARA_DEV_DEF, *PPARA_DEV_DEF;

/**
 * @brief 
 * 渠道与区域
 */
typedef struct
{
    char     region[64];
    char     channelCode[64];
    char     walletCode[64];
    char     pageName[64];
}__attribute__((aligned(1), packed))CHANNEL_PAYMENT_DEF,*PCHANNEL_PAYMENT_DEF;


extern PARAINTER_DEF *para_getInterInfor(void);
extern PARA_DEV_DEF *para_getDevInfor(void);
extern void para_deviceInit(void);
extern int32_t lyy_dev_read(PPARA_DEV_DEF arg);
extern int32_t lyy_dev_write(PPARA_DEV_DEF arg);
extern void set_serAddr(PPARAGATE_DEF gate, uint8_t type);
extern int                      get_dev_type_code(void);
extern PAY_PACKAGES_DEF         *get_pay_packages(void);
extern PAY_PACKAGES_NDJ_DEF     *get_pay_ndj_packages(void);
extern PAY_PARAM_DEF            *get_pay_param(void);

extern uint8_t                  ndj_get_lane_enable(uint8_t x);
extern PLANE_INFOR_TABLE_DEF    ndj_net_lane_infor(void);

extern char                     *get_money_unit(void);

#if 0

extern lyy_int32_t      lyy_dev_read(PPARA_DEV_DEF arg);
extern lyy_int32_t      lyy_dev_write(PPARA_DEV_DEF arg);
extern lyy_int32_t      lyy_acc_read(void *arg);
extern lyy_int32_t      lyy_acc_write(void *arg);

extern lyy_int8_t       uart_brt_get(uint32_t brt);
extern uint32_t         uart_brt_set(uint8_t level);


extern void             netType_set(uint8_t type);


extern char             *deviceid_get(void);
extern PARAINTER_DEF    *para_getInterInfor(void);
extern PARA_FLOW_DEF    *para_getFlowInfor(void);
extern PARAGATE_DEF     *para_getGateInfor(void);

extern PARAAES_DEF      *para_getAesInfor(void);

extern uint16_t     para_getPulseWidth(void);
extern uint16_t     para_getPulseGap(void);

extern PARA_DEV_DEF     *para_getDevInfor(void);


extern int              deviceID_isok(void);
extern void             lyy_init_para(void);
extern void             device_param_default(void);
extern void             para_devid_openUart(void);


extern void             para_clearDevpara(void);
extern void             para_clearDevparaIntoChk(void);
extern void             para_uart_clearDevpara(void);
extern void             business_set(uint8_t type);
extern void             para_setFlowInfor(PARA_FLOW_DEF *flowInfo);
extern void             set_serAddr(PPARAGATE_DEF gateInfor,uint8_t type);
extern lyy_int16_t      event_writeParaRegist(void *arg,uint16_t res,lyy_int32_t (*update)(void *,uint16_t ));

extern void             lyy_socket_url(char *url,uint16_t *port,uint8_t *type);
extern void             iot_gate_infor_set(uint16_t port , char *url);

extern void             para_waitDevid(void *arg);



extern PLANE_INFOR_TABLE_DEF get_ndj_lane_infor(void);
extern PLANE_INFOR_TABLE_DEF ndj_net_lane_infor(void);
extern uint8_t          ndj_get_lane_enable(uint8_t x);


extern void ndj_write_pay_para(void);
extern lyy_int8_t deviceid_set_format(int32_t id,uint8_t *cmd,uint16_t len);

extern PAY_PARAM_DEF        *get_pay_param(void);
extern PAY_PACKAGES_DEF     *get_pay_packages(void);
extern PAY_PACKAGES_NDJ_DEF *get_pay_ndj_packages(void);
extern INFOR_BUSINESS       *get_business_infor(void);

extern int lyy_pay_param_read(PAY_PARAM_DEF *arg);
extern char *device_type_get(void);
extern void device_type_set(char *type);
extern int  get_dev_type_code(void);
extern char *get_money_unit(void);
extern lyy_int32_t para_deletefile(const char *filename);

#endif

#endif






