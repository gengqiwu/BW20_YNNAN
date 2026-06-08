/*
 * @author: zhuyalei
 * @Date: 2026-04-22 10:00:00
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2026-04-22 10:00:00
 * @Description: 探虎洗车机串口通信协议
 */
#ifndef __LYY_XCJ_H__
#define __LYY_XCJ_H__

#include "stdint.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

//帧头帧尾
#define XCJ_FRAME_HEAD                      0xBB
#define XCJ_FRAME_END                       0xDD

//发送方标识
#define XCJ_SENDER_BOX                      0x01    //终端/盒子
#define XCJ_SENDER_XCJ                      0x02    //洗车机主板
#define XCJ_SENDER_SERVER                   0x03    //支持端/后台

//命令字 CMD
#define XCJ_CMD_LOGIN                       0x31    //登录命令,查询信号
#define XCJ_CMD_CARD_PAY                    0x32    //上传卡号(刷卡消费)
#define XCJ_CMD_MEMBER_PAY                  0x33    //上传会员号(扫码/APP支付)
#define XCJ_CMD_PAY_CONTROL                 0x41    //支付指令(盒子->洗车机)
#define XCJ_CMD_FORCE_CTRL                  0x42    //强制控制(启停/暂停)
#define XCJ_CMD_SET_PARAM                   0x43    //设置设备参数
#define XCJ_CMD_GET_INFO                    0x44    //获取设备信息

//洗车机通讯状态
enum XCJ_COMM_STA_E
{
    XCJ_COMM_NOT_CONNECTED = 0x00,     //未连接
    XCJ_COMM_RECEIVED      = 0x01,     //已收到终端应答
    XCJ_COMM_SERVER_OK     = 0x10,     //与后台通讯正常
    XCJ_COMM_SERVER_FAIL   = 0x11,     //与后台通讯异常
};

//设备系统运行状态
enum XCJ_RUN_STA_E
{
    XCJ_RUN_STA_IDLE          = 0x00,  //待机状态
    XCJ_RUN_STA_IN_USE        = 0x01,  //设备正在进行(使用状态)
    XCJ_RUN_STA_FINISHING     = 0x02,  //使用已结束(进行状态)
    XCJ_RUN_STA_NOT_READY     = 0x03,  //设备未就绪状态
    XCJ_RUN_STA_SUSPENDED     = 0x04,  //设备暂停(暂停营业)
    XCJ_RUN_STA_FAULT         = 0x05,  //设备故障
    XCJ_RUN_STA_MAINTAIN      = 0x06,  //设备维护中
    XCJ_RUN_STA_WAIT_CARD     = 0x07,  //刷卡机等待下次卡片信息
    XCJ_RUN_STA_CARD_INVALID  = 0x08,  //卡无效余额不足准备退款
    XCJ_RUN_STA_COMM_LOST     = 0x09,  //通讯失效需重新登录
    XCJ_RUN_STA_LOGIN_TIMEOUT = 0x0A,  //登录超时
};

//消费类型
enum XCJ_CONSUME_TYPE_E
{
    XCJ_CONSUME_ONCE   = 0x01,   //一次性消费选
    XCJ_CONSUME_COUNT  = 0x02,   //按次消费选(按次消费为会员剩余)
};

//启动/停止
enum XCJ_CTRL_ACTION_E
{
    XCJ_CTRL_STOP  = 0x00,  //停止洗车
    XCJ_CTRL_START = 0x01,  //启动洗车
};

//强制控制动作
enum XCJ_FORCE_ACTION_E
{
    XCJ_FORCE_START = 0x01,  //强制启动设备
    XCJ_FORCE_PAUSE = 0x02,  //强制暂停设备
    XCJ_FORCE_STOP  = 0x04,  //强制停止设备
};

//停止原因
enum XCJ_STOP_REASON_E
{
    XCJ_STOP_NORMAL       = 0x00,  //正常停止洗车
    XCJ_STOP_CARD_INVALID = 0x01,  //卡无效
    XCJ_STOP_NO_MONEY     = 0x02,  //余额不足
};

//设备工作状态
typedef struct
{
    uint8_t loginMark[2];      // 登录标识
    uint8_t commSta;          //洗车机主板通讯状态  00 未连接  01 已连接  10 正在重连  11 从新连上
    uint8_t faultSta[2];     // 故障状态   00为无故障
    uint8_t workSta[2];     // 设备工作状态
    uint8_t devSta;        // 设备运行状态  00 为待机态，可以用，别的抓过你太都不能用
    //uint8_t retain;       //保留
}__attribute__((aligned(1), packed)) TYPE_XCJ_DEV_STA, *PTYPE_XCJ_DEV_STA;

//洗车机参数
typedef struct
{
    uint16_t foamPrice;            //泡沫单价 x0.001元
    uint16_t waxPrice;             //蜡水单价 x0.001元
    uint16_t waterPrice;           //清水单价 x0.001元
    uint16_t coatPrice;            //镀膜单价 x0.001元
    uint16_t dryPrice;             //风干单价 x0.001元
    uint16_t warmPrice;            //暖风单价 x0.001元
    uint16_t washPrice;            //洗车单价 x0.001元
    uint16_t openTime;             //开放时间
    uint16_t closeTime;            //关闭时间
    uint16_t waitTime;             //等待时间(秒)
    uint16_t waitTimeoutPrice;     //超时等待每分钟消费 x0.001元
    uint8_t  adCount;              //广告数量 0~9
    uint8_t  tempMin;              //温度下限 0~99
    uint8_t  tempMax;              //温度上限 0~99
    uint8_t  lightLevel;           //灯光亮度 0~99
    uint8_t  devEnable;            //启停设备 0x00停 0x01启
    uint8_t  bizEnable;            //启停营业 0x00停 0x01启
    uint8_t  serverIp[4];          //服务器IP
    uint8_t  washWaitTimeout;      //洗车等待时间 单位分钟 1~240
}__attribute__((aligned(1), packed)) TYPE_XCJ_PARAM, *PTYPE_XCJ_PARAM;

//支付指令参数(CMD 0x41 盒子->洗车机)
typedef struct
{
    uint8_t  memberCode[10];       //10位会员唯一编码
    uint8_t  amount[3];            //会员金额 单位分 3字节
    uint8_t  action;               //启动/停止 0x00停止 0x01启动
    uint8_t  stopReason;           //停止原因
    uint8_t  consumeType;          //消费类型 0x01一次性 0x02按次消费
    uint8_t  consumedMin[3];        //消费金额下限 单位分 3字节
    uint8_t  consumedMax[3];       //消费金额上限 单位分 3字节
    uint8_t  time;                 //赠送元素
}__attribute__((aligned(1), packed)) XCJ_PAY_CMD_DEF, *PXCJ_PAY_CMD_DEF;

//洗车机应答(CMD 0x41 洗车机->盒子)
typedef struct
{
    uint8_t  amount[3];            //金额 单位分 3字节
    uint8_t  result;               //执行结果 0x00不成功 0x01成功
    uint8_t  failReason;           //失败原因 bit位
    uint8_t  consumeType;          //消费类型
    uint8_t  consumedMin[3];       //消费金额下限 3字节
    uint8_t  consumedMax[3];       //消费金额上限 3字节
    uint8_t  retain;                // 保留
}__attribute__((aligned(1), packed)) XCJ_PAY_REPLY_DEF, *PXCJ_PAY_REPLY_DEF;

//强制控制应答(CMD 0x42 洗车机->盒子)
typedef struct
{
    uint8_t result;                //0x00失败 0x01成功
    uint8_t reason;                //原因
}__attribute__((aligned(1), packed)) XCJ_FORCE_REPLY_DEF, *PXCJ_FORCE_REPLY_DEF;

extern TYPE_XCJ_DEV_STA xcj_devSta;

extern void lyy_device_xcj_init(void);
extern int xcj_pay_apply(void);
extern int xcj_pay_start(uint16_t amount, uint8_t time);
extern void xcj_packet_parse(uint8_t *package, uint16_t len);
extern void xcj_send_pay_cmd(uint16_t amount, uint8_t time);
extern void xcj_send_force_ctrl(uint8_t action);
extern void xcj_send_set_param(TYPE_XCJ_PARAM *param);
extern void xcj_send_get_info(void);

#endif
