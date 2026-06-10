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

#define INIT_NET_COIN_MARK      0xA0 //自增码初始值

#define DEV_SELL_CMD                        (0xC9)
#define DEVICE_NDJ_CMD                      (0xCC)

#define THING_IOT_BOARD_PATH        "BOARD_OTA"

extern SemaphoreHandle_t xFlashMutex;

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
}LyyHwMark_t;

extern LyyHwMark_t     g_xlyyHwMark;
extern DEVICE_COM_DEF  tdevCom;

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
extern uint16_t login_mark_get(void);

extern int8_t device_send_cmd_packet(uint8_t cmd , uint8_t *pack_data,uint16_t len);

#endif


