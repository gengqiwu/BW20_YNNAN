/*
 * @author: e.he
 * @Date: 2022-10-09 11:20:15
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-08 18:06:10
 * @Description: 
 */
#ifndef __SYS_DEVICE_H__
#define __SYS_DEVICE_H__

#include "stdint.h"

#include "HAL_System.h"
#include "hal_sem.h"

#define INIT_NET_COIN_MARK      0xA0 //自增码初始值

#define DEV_SELL_CMD                        (0xC9)
#define DEVICE_NDJ_CMD                      (0xCC)

#define HGJ_PARAM_CMD_QUERY 0x05
#define HGJ_PARAM_CMD_SET 0x06
#define XYJ_GLOBAL_PARAM_CMD_QUERY 0x30
#define XYJ_GLOBAL_PARAM_CMD_SET 0x31
#define XYJ_STEP1_PARAM_CMD_QUERY 0x32
#define XYJ_STEP1_PARAM_CMD_SET 0x33
#define XYJ_STEP2_PARAM_CMD_QUERY 0x34
#define XYJ_STEP2_PARAM_CMD_SET 0x35

#define THING_IOT_BOARD_PATH         "BOARD_OTA"
#define THING_IOT_SCREEN_PATH        "SCREEN_OTA"

// #define ACCOUNT_LENGTH_CHECK    //美金、奥宇等某些存量主板账目指令长度有问题，特殊版本下不开启账目长度校验

enum 
{   
    TYPE_QUREY_LOGIN_FLAG=1,
    TYPE_QUREY_PAY_APPLY,
};



enum 
{
    FLOW_SWITCH_OFF=0,
    FLOW_SWITCH_ON =1,
};

enum 
{
    FLOW_SWITCH_VAL_OFF=0,
    FLOW_SWITCH_VAL_ON =1,
};

enum FLOW_STA_E
{
    FLOW_STA_COUNT = 0,
    FLOW_STA_IS_RESET ,
    FLOW_STA_SW_CLOSE ,
};

extern char lane_get_lane_id[64];





#define OFFLINE_ACCOUNT_PATH   "OFFLINE_ACCOUNT_DATA"
#define OFFLINE_ACCOUNT_MARK_INIT 'D'

// 时间戳
typedef struct
{
    uint64_t timestamp;         // 时间戳
    uint64_t device_send_time;  // 设备发送时间戳
    uint64_t server_receive_time;    // 服务器接收时间戳
    uint64_t server_send_time;     // 服务器发送时间戳
} __attribute__((aligned(1), packed)) DEVICE_NTP_DEF;
extern DEVICE_NTP_DEF ntp_time_t;

// 离线账目临时数据
typedef struct
{
    uint8_t account_infor_flag;  //设备是否上报账目标志 0 上报  1不上报
    char  unique_code[12];        // 记录后台下发清除账目的唯一标识码
} __attribute__((aligned(1), packed)) DEVICE_ACCOUNT_DEF;

extern DEVICE_ACCOUNT_DEF account_data_t;

// 环形队列元数据，存在分区最开头
typedef struct
{
    char  mark;          // 判断是不是"D",是说明有效数据，非"D"说明无效数据
    uint32_t write_idx;     // 写入下标：下一条新账单存这里
    uint32_t read_idx;      // 读取下标：第一条待上报账单
    uint32_t total_pending; // 当前未上报账单总数
    uint8_t crc;           // 元数据自身CRC校验，防止半写损坏  预留，暂时不使用
} __attribute__((aligned(1), packed)) CacheMeta_t;

// 支付方式枚举
typedef enum
{
    PAY_METHOD_ONLINE = 1,          // 线上支付
    PAY_METHOD_OFFLINE = 2,         // 线下支付
    PAY_METHOD_POS_DEBIT_CARD = 3,  // POS储蓄卡支付
    PAY_METHOD_POS_CREDIT_CARD = 4, // POS信用卡支付
    PAY_METHOD_POS_ALL = 5,         // POS支付
    PAY_METHOD_CHANGE = 6           // 找零
} PayMethodEnum;

typedef enum
{
    MSG_TYPE_COIN_OUT = 1, // 出币
    MSG_TYPE_AMOUNT = 2,   // 金额
    MSG_TYPE_GIFT_OUT = 3, // 出礼
    MSG_TYPE_COIN_IN = 4 ,  // 投币
    MSG_TYPE_PULSE_COIN = 5 ,  //  脉冲投币
    MSG_TYPE_PULSE_GIFT = 6  //  脉冲出礼
} MsgTypeEnum;

// 单条缓存条目
typedef struct
{
    uint32_t increment;     //  增量
    uint32_t total;         //  总量
    uint8_t position_x;     // x轴
    uint8_t position_y;     // y轴
    uint8_t type;           //  类型   数据类型：1.出币，2.金额，3.出礼，4.投币    后台为字符串类型需要转换
    uint8_t pay_method;     //  支付方式  数据类型：1.线上支付，2.线下支付，3.POS储蓄卡支付，4.pos信用卡支付，5.pos支付，6.找零，  后台为字符串类型需要转换
    char unique_code[7];    // 唯一标识码 
    uint64_t time;          // 时间戳  后台为字符串类型需要转换
    uint8_t crc;            // 本条数据CRC，校验Flash写入是否损坏  预留，暂时不使用
} __attribute__((aligned(1), packed)) BillCacheItem_t;

#define BILL_ITEM_LEN  sizeof(BillCacheItem_t)
#define MAX_BILL_NUM 2000   //最大存储数目

//正向主板协议
typedef struct
{
    uint8_t     head;
    uint8_t     len;
    uint8_t     index;
    uint8_t     cmd;
    uint8_t     data[1255];
    uint8_t     check;
    uint8_t     end;
}__attribute__((aligned(1), packed))DEVICE_PACK_DEF,*PDEVICE_PACK_DEF;

// 德峰售水机协议
typedef struct
{
    uint8_t head;
    uint8_t len;
    uint8_t cmd;
    uint8_t data[1255];
    uint8_t check;
    uint8_t end;
} __attribute__((aligned(1), packed)) DEVICE_DF_PACK_DEF, *PDEVICE_DF_PACK_DEF;

// 反向协议
typedef struct
{
    uint8_t cmdID[6];
    uint8_t data[600];
}__attribute__((aligned(1), packed)) OPPOSITE_PACK_DEF,*POPPOSITE_PACK_DEF;


//主板通信流程控制
typedef struct
{
    uint8_t             qurey_type;         //查询主板链接类型
    uint8_t             netCoin_mark;       //上分标识
    void                *qurey_timer_id;
    void                *update_accPara_timerid;
    uint8_t             qurey_num;
    
    uint8_t             mark_cmd;
    SemaphoreHandle_t   sem;
    uint8_t             unique[6];
    uint8_t             start_result;
    uint16_t            qurey_flag;
    uint8_t             dev_apply_status;
}__attribute__((aligned(1), packed))DEVICE_COM_DEF,*PDEVICE_COM_DEF;  

/**
 * @brief 
 * 启动相关参数
 */
typedef struct
{
    SemaphoreHandle_t   apply_sem;
    SemaphoreHandle_t   start_sem;
    int                 apply_result;
    int                 start_result;
    uint8_t             ndj_apply_id;
    uint8_t             ndj_start_id;
    uint8_t             ndj_cmd_id[10][6];
}__attribute__((aligned(1), packed))APPLY_START_DEF,*PAPPLY_START_DEF;
extern APPLY_START_DEF apply_start_t;
/**
 * @brief 
 * 指令标识
 */
typedef union 
{
    char        id[6];
    uint32_t    cnt;
}CMD_ID_U;





//主板升级相关
#define BOARD_UP_FILE_NAME_LEN      20
#define BOARD_UP_PACKET_LEN         512
#define BOARD_UP_VERSION_LEN        50

enum BOARD_UP_STATE_E
{
    BOARD_UP_IDLE = 0,      //空闲
    BOARD_UP_SEND,          //发送
    BOARD_UP_FINISH ,       //结束
};



typedef struct
{
    uint16_t    size;                            //文件总大小-
    uint16_t    total;                           //文件总分包数
    uint16_t    number;                          //文件当前分包数
    uint8_t     result;
}__attribute__((aligned(1), packed))BOARD_OTA_DATA_ACK,*PBOARD_OTA_DATA_ACK;
/**
 * @brief 
 * 数据包格式
 */
typedef struct
{
    uint16_t    frame_len;
    uint8_t     sub_cmd;
    uint8_t     name_len;
    char        file_name[BOARD_UP_FILE_NAME_LEN];       //文件名称
    uint16_t    file_size;                               //文件总大小-
    uint16_t    packet_sum;                             //文件总分包数
    uint16_t    packet_num;                             //文件当前分包数
    uint16_t    data_len;                               //数据长度
    uint8_t     file_data[512];                         //数据区
}__attribute__((aligned(1), packed))BOARD_OTA_DATA_DEF,*PBOARD_OTA_DATA_DEF;

//登录相关的变量
typedef struct
{
    uint16_t             login_mark;
}LOGIN_VAR,*PLOGIN_VAR;

typedef struct {

    uint8_t             ucGetLoginFlag;
    uint16_t            ucLoginMark;
    uint8_t             ucPaySem;
    SemaphoreHandle_t   pxPayApply;
    int                 applyResult;
    int                 startResult;

    uint8_t             ndj_apply_id;
    uint8_t             ndj_start_id;
    uint8_t             ndj_cmd_id[9][6];  // 8个仓位、6为的id使用（ID=1-8）

    uint8_t             dfssj_apply_id;
    uint8_t             dfssj_start_id;
    uint8_t             dfssj_cmd_id[11][6]; // 10个出水口、6为的id使用（ID=1-8）
}LyyHwMark_t;

extern LyyHwMark_t     g_xlyyHwMark;
extern DEVICE_COM_DEF  tdevCom;

enum
{
    BOARD_OTA_STATUS_IDLE = 0x00,
    BOARD_OTA_STATUS_INIT,
    BOARD_OTA_STATUS_DATA_SEND,
    SCREEN_OTA_BAUD_SET,
    SCREEN_OTA_STATUS_INIT,
    SCREEN_OTA_STATUS_DATA_SEND,
};

typedef struct
{
    uint8_t     status;             //状态
    uint32_t    file_size;          //文件大小
    uint16_t    number;
    uint16_t    sum;
    char        file_name[21];      //文件名称
    char        version[21];        //版本
    uint32_t    offset;        // 偏移量
}__attribute__((aligned(1), packed))BOARD_OTA_PARAM_DEF,*PBOARD_OTA_PARAM_DEF;
extern BOARD_OTA_PARAM_DEF     board_ota_param_t;



extern void lyy_init_device(void);
extern void device_qureyConect(uint8_t type);
extern char device_buildPacket(PDEVICE_PACK_DEF pack);
extern void device_packet_data_length(uint8_t cmd, uint8_t *data, uint16_t dataLen);
extern int board_ota_status_send(void);
extern void board_ota_param_init(char *version,char *file_name,uint32_t file_size);
extern void screen_ota_param_init(char *version, char *file_name, uint32_t file_size);
extern uint16_t login_mark_get(void);

extern int8_t device_send_cmd_packet(uint8_t cmd , uint8_t *pack_data,uint16_t len);
extern int8_t device_send_dfssj_packet(uint8_t *pack_data, uint16_t len);

/* 离线账单环形缓存接口 */
extern int      bill_cache_clear(void);                 /* 删除整个账单缓存区域 */
extern int      bill_cache_push(BillCacheItem_t *item); /* 新增账单存入环形缓存 */
extern uint32_t bill_cache_get_pending(void);           /* 查询当前积压条数 */

extern uint64_t get_timestamp(void);
extern char *get_offline_accounts_pay_method_type(uint8_t code);
extern char *get_offline_accounts_data_type(uint8_t code);
extern void offline_accounts_save(uint32_t increment, uint32_t total, uint8_t data_type, uint8_t pay_method);
extern void offline_accounts_save_ndj(uint32_t increment, uint32_t total, uint8_t position_x, uint8_t data_type, uint8_t pay_method);

#endif


