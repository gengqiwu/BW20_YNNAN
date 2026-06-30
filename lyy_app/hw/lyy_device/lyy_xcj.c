/*
 * @author: zhuyalei
 * @Date: 2026-04-22 10:00:00
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2026-04-22 10:00:00
 * @Description: 探虎洗车机串口通信协议实现
 * 协议帧格式: 0xBB | Length | Index | CMD | Data[0](sender) | Data[1](reserved=0x00) | Data[2..N-1] | Check | 0xDD
 * Check = 16位校验和(sum), 取低8位
 * 波特率: 38400, TTL, 小端模式
 */
#include "lyy_xcj.h"
#include "lyy_xyj.h"
#include "lyy_device.h"
#include "lyy_tjc.h"

#include "APP_Param.h"
#include "APP_Pluse.h"
#include "APP_Detect.h"
#include "APP_DrillingMode.h"

#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_WIFI.h"
#include "HAL_Uart.h"
#include "HAL_System.h"
#include "HAL_Flash.h"
#include "HAL_Sem.h"
#include "thing_event.h"

#include "thing_iot.h"
#include "thing_mqtt.h"
#include "lyy_para.h"

#include "utils.h"

static SEM_TYPE xcj_start_sem;
static SEM_TYPE xcj_checkStaus_sem;
TYPE_XCJ_DEV_STA xcj_devSta = {0};

static int terminal_xcj_send_frame(uint8_t cmd, uint8_t *data, uint16_t dataLen);
void xcj_send_get_info(void);

/**
 * @brief
 * 探虎洗车机16位校验和,取低8位
 * 将从S_Num到L_Num的所有字节相加,取反,取低8位
 * @param DoStr 数据缓冲区
 * @param S_Num 起始索引
 * @param L_Num 结束索引
 * @return uint8_t 校验值
 */
static uint8_t checksum8(uint8_t *DoStr, uint8_t S_Num, uint8_t L_Num)
{
    uint8_t Do_i;
    uint16_t xorsum = 0;

    for (Do_i = S_Num; Do_i <= L_Num; Do_i++)
    {
        xorsum = xorsum + DoStr[Do_i];
    }

    xorsum = 0 - xorsum;

    return (uint8_t)(xorsum & 0x00ff);
}

/**
 * @brief
 * 组帧并发送到洗车机主板
 * 帧格式: 0xBB | Length | Index | CMD | Data[] | Check | 0xDD
 * Length = Index + CMD + Data[] + Check
 * @param index 发送方标识 0x01=盒子
 * @param cmd 命令字
 * @param data 数据区(Data[2]开始)
 * @param dataLen 数据区长度(不含Data[0]和Data[1])
 * @return int 发送结果
 */
static int terminal_xcj_send_frame(uint8_t cmd, uint8_t *data, uint16_t dataLen)
{
    //总帧长度 = Head(1) + Length(1) + Index(1) + CMD(1) + Data[0](1) + Data[1](1) + dataLen + Check(1) + End(1)
    uint16_t frameLen = dataLen + 8;
    uint8_t *txbuff = pvPortMalloc(frameLen);

    if (txbuff == NULL)
        return -1;

    memset(txbuff, 0, frameLen);

    txbuff[0] = XCJ_FRAME_HEAD;         //0xBB
    txbuff[1] = dataLen + 5;            //Length = Index(1) + CMD(1) + Data[0..N](dataLen+2) + Check(1)
    txbuff[2] = XCJ_SENDER_BOX;         //Index: 0x01=盒子(发送方)
    txbuff[3] = cmd;                     //CMD
    txbuff[4] = XCJ_SENDER_XCJ;         //Data[0] 接收方=洗车机主板
    txbuff[5] = 0x00;                    //Data[1] 保留

    if (dataLen > 0 && data != NULL)
    {
        memcpy(&txbuff[6], data, dataLen);
    }

    //Check = 校验和(从第1字节到倒数第2字节)
    txbuff[frameLen - 2] = checksum8(txbuff, 1, frameLen - 3);
    txbuff[frameLen - 1] = XCJ_FRAME_END; //0xDD

    int ret = terminal_device_data(txbuff, frameLen);

    vPortFree(txbuff);

    return ret;
}


/**
 * @brief
 * 回复洗车机登录命令(CMD 0x31)
 * 盒子 -> 洗车机: 回复设备ID,时间,信号,服务器状态
 * @param loginMark 登录标识
 */
static void xcj_reply_login(void)
{
    uint8_t data[18] = {0};
    char *devid = APP_pcGetParamDevId();

    //Data[2-9]: 8位设备唯一码
    memcpy(&data[0], devid, 8);

    //Data[10-13]: 年月日时 (暂填固定值,后续可从RTC获取)
    data[8] = 26;   //年 20XX
    data[9] = 4;    //月
    data[10] = 22;  //日
    data[11] = 10;  //时

    //Data[14-15]: 分秒
    data[12] = 0;   //分
    data[13] = 0;   //秒

    //Data[16]: 信号值
    data[14] = HAL_lGetWifiRssiToCsq();

    //Data[17]: 是否在线 0x01在线 0x00离线
    data[15] = lyy_mqtt_get_status() ? 0x01 : 0x00;

    //Data[18]: 保留
    data[16] = 0x00;

    terminal_xcj_send_frame( XCJ_CMD_LOGIN, data, sizeof(data));
}

/**
 * @brief
 * 处理登录命令(CMD 0x31) 洗车机->盒子
 * 解析洗车机上传的登录标识、通讯状态、系统状态、工作状态
 * @param data 数据区(Data[2]开始)
 * @param len 数据长度
 */
static void xcj_parse_login(uint8_t *data, uint16_t len)
{
    if (len < sizeof(TYPE_XCJ_DEV_STA)) return;

    memcpy(&xcj_devSta.loginMark[0], data, sizeof(TYPE_XCJ_DEV_STA));

    // 回复洗车机
    xcj_reply_login();
}

/**
 * @brief
 * 处理刷卡消费(CMD 0x32) 洗车机->盒子
 * 洗车机上传10位卡号,盒子回复对应金额和启动/停止
 * @param data 数据区(Data[2]开始)
 * @param len 数据长度
 */
static void xcj_parse_card_pay(uint8_t *data, uint16_t len)
{
    (void)len;
    uint8_t reply[22] = {0};

    //Data[1]=0xA1 标识刷卡消费
    //Data[2-11]: 10位卡号
    uint8_t cardCode[10] = {0};
    memcpy(cardCode, &data[0], 10);

    LYY_LOGD("xcj_card_pay code:%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
             cardCode[0], cardCode[1], cardCode[2], cardCode[3], cardCode[4],
             cardCode[5], cardCode[6], cardCode[7], cardCode[8], cardCode[9]);

    //回复: Data[1]=0xA1 + 10位卡号 + 金额(3字节) + 启停 + 停止原因 + 消费类型(0x02刷卡) + 已消费(3) + 消费上限(3) + 保留
    reply[0] = 0xA1;                       //标识刷卡消费
    memcpy(&reply[1], cardCode, 10);       //10位卡号
    //金额暂填0, 由业务层计算
    reply[11] = 0x00;                      //金额低字节
    reply[12] = 0x00;                      //金额中字节
    reply[13] = 0x00;                      //金额高字节
    reply[14] = XCJ_CTRL_START;            //启动洗车
    reply[15] = XCJ_STOP_NORMAL;           //停止原因
    reply[16] = 0x02;                      //刷卡消费
    reply[17] = 0x00;                      //已消费金额低
    reply[18] = 0x00;                      //已消费金额中
    reply[19] = 0x00;                      //已消费金额高
    reply[20] = 0x00;                      //已消费金额上限低
    reply[21] = 0x00;                      //已消费金额上限中

    //TODO: 实际业务中需要根据卡号查询余额,计算金额
    //terminal_xcj_send_frame( XCJ_CMD_CARD_PAY, reply, sizeof(reply));
}

/**
 * @brief
 * 处理扫码/APP支付(CMD 0x33) 洗车机->盒子
 * 洗车机上传会员号,盒子回复对应金额和扣款结果
 * @param data 数据区(Data[2]开始)
 * @param len 数据长度
 */
static void xcj_parse_member_pay(uint8_t *data, uint16_t len)
{
    (void)len;
    uint8_t reply[17] = {0};

    //Data[2-11]: 10位会员唯一编码
    uint8_t memberCode[10] = {0};
    memcpy(memberCode, &data[0], 10);

    //Data[12]: 消费类型
    uint8_t consumeType = data[10];

    //Data[13-15]: 会员剩余金额(3字节)
    uint32_t remainBalance = data[11] + (data[12] << 8) + (data[13] << 16);

    LYY_LOGD("xcj_member_pay type:%d remain:%d", consumeType, remainBalance);

    //回复: 10位会员号 + 消费类型 + 3字节余额 + 结果
    memcpy(&reply[0], memberCode, 10);     //10位会员号
    reply[10] = consumeType;               //消费类型
    reply[11] = data[11];                  //余额低字节
    reply[12] = data[12];                  //余额中字节
    reply[13] = data[13];                  //余额高字节
    reply[14] = 0x01;                      //结果: 0x01成功 0x00不成功

    //TODO: 实际业务中需要根据会员号查询余额,验证扣款

    //terminal_xcj_send_frame( XCJ_CMD_MEMBER_PAY, reply, sizeof(reply));
}

/**
 * @brief
 * 处理支付指令应答(CMD 0x41) 洗车机->盒子
 * 洗车机回复盒子下发的支付指令执行结果
 * @param data 数据区(Data[2]开始)
 * @param len 数据长度
 */
static void xcj_parse_pay_reply(uint8_t *data, uint16_t len)
{
    (void)len;
    PXCJ_PAY_REPLY_DEF pay_reply = (PXCJ_PAY_REPLY_DEF)data; 

    if (pay_reply->result==0x01)
    {
        apply_start_t.start_result = 0x01; // 0x00-失败 0x01-成功
    }
    else
    {
        apply_start_t.start_result = 0x00;
    }
    LYY_LOGD("apply_start_t.start_result[%d]", apply_start_t.start_result);
    if (xcj_start_sem != NULL)
    {
        lyy_semSignalV2(xcj_start_sem);
        LYY_LOGD("xcj_start_sem\r\n");
    }
}

/**
 * @brief
 * 处理强制控制应答(CMD 0x42) 洗车机->盒子
 * @param data 数据区(Data[2]开始)
 * @param len 数据长度
 */
static void xcj_parse_force_reply(uint8_t *data, uint16_t len)
{
    (void)len;
    XCJ_FORCE_REPLY_DEF reply = {0};

    //Data[2]: 是否成功 0x00失败 0x01成功
    reply.result = data[0];

    //Data[3]: 成功/失败原因
    reply.reason = data[1];

    LYY_LOGD("xcj_force_reply result:%d reason:%d", reply.result, reply.reason);

    if (xcj_start_sem != NULL)
    {
        lyy_semSignalV2(xcj_start_sem);
        LYY_LOGD("xcj_start_sem\r\n");
    }
}

/**
 * @brief
 * 处理参数设置应答(CMD 0x43) 洗车机->盒子
 * 洗车机回复相同的参数数据+确认字节
 * @param data 数据区(Data[2]开始)
 * @param len 数据长度
 */
static void xcj_parse_set_param_reply(uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
    LYY_LOGD("xcj_set_param_reply\r\n");

    if (len < 35)
        return;

    //Data[34]: 确认 0x00收到但不成功 0x01收到且保存成功
    uint8_t confirm = data[33];
    //Data[35]: 不成功原因
    uint8_t reason = data[34];

    LYY_LOGD("xcj_set_param confirm:%d reason:%d", confirm, reason);
}

/**
 * @brief
 * 处理获取设备信息应答(CMD 0x44) 洗车机->盒子
 * 包含设备所有状态、参数、版本信息
 * @param data 数据区(Data[2]开始)
 * @param len 数据长度
 */
static void xcj_parse_get_info_reply(uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
    LYY_LOGD("xcj_get_info_reply len:%d\r\n", len);

    //解析设备状态
    memcpy(&xcj_devSta.loginMark[0], data, sizeof(TYPE_XCJ_DEV_STA));

    //各种参数信息，需要时在处理


    if (xcj_checkStaus_sem != NULL)
    {
        lyy_semFreeV2(xcj_checkStaus_sem);
        LYY_LOGD("xcj_checkStaus_sem\r\n");
    }


}

/**
 * @brief
 * 查询状态定时任务
 * @param arg 未使用
 */
void thread_xcj_inquire_status(void *arg)
{
    (void)arg;
    uint8_t ret = 0;

    while (1)
    {

        lyy_semNewV2(&xcj_checkStaus_sem, 0);
        xcj_send_get_info();
        // vTaskDelay(2000 / portTICK_PERIOD_MS);
        // xcj_pay_start(10,20);
        // vTaskDelay(2000 / portTICK_PERIOD_MS);
        //xcj_reply_login();
        ret = lyy_semWaitTryV2(xcj_checkStaus_sem, 3000); // 等待主板启动应答
        lyy_semFreeV2(xcj_checkStaus_sem);
        xcj_checkStaus_sem = NULL;

        if (ret > 0)
        {
            // 更新状态
        }
        else
        {
            // 查询状态应达超时，状态置为无效端口
            xcj_devSta.devSta = 0x09;
        }
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief
 * 初始化探虎洗车机
 */
void lyy_device_xcj_init(void)
{
    xTaskCreate(thread_xcj_inquire_status,
                "thread_xcj_inquire_status",
                1024 * 2,
                NULL,
                5,
                NULL);
}

/**
 * @brief
 * 支付申请---探虎洗车机
 * 查询洗车机当前状态,判断是否可以支付
 * @return int >0 成功 =0 失败
 */
int xcj_pay_apply(void)
{
    uint8_t ret = 0;
    LYY_LOGD("xcj_pay_apply");

    if (xcj_devSta.devSta==0) ret = 1;
    else  ret = 0;

    return ret;
}

/**
 * @brief
 * 支付启动---探虎洗车机
 * 下发支付指令,等待洗车机启动应答
 * @param mode 未使用(洗车机无模式选择)
 * @return int >0 成功 =0 失败
 */
int xcj_pay_start(uint16_t amount, uint8_t time)
{
    uint8_t ret = 0;

    lyy_semNewV2(&xcj_start_sem, 0);
    xcj_send_pay_cmd(amount,time);         // 下发
    ret = lyy_semWaitTryV2(xcj_start_sem, 3000);             //等待启动应答
    lyy_semFreeV2(xcj_start_sem);
    xcj_start_sem = NULL;

    if (ret > 0)
    {
        //收到主板应答
        if (apply_start_t.start_result == 0x01)
        {
           //启动成功
        }
        else
        {
           //启动失败
        }
    }
    else
    {
    }
    return ret;
}

/**
 * @brief
 * 发送支付指令(CMD 0x41) 盒子->洗车机
 * @param pay 支付参数
 */
void xcj_send_pay_cmd(uint16_t amount,uint8_t time)
{
    //Data区: 10字节会员码 + 3字节金额 + 启停 + 停止原因 + 消费类型 + 3字节已消费 + 3字节上限 + 赠送 = 24字节
    XCJ_PAY_CMD_DEF pay = {0};

    //Data[2-11]: 10位会员唯一编码
    pay.memberCode[7] = 1;
    pay.memberCode[8] = 2;
    pay.memberCode[9] = 3;

    // Data[12-14]: 会员金额(3字节)
    pay.amount[0] = (100 * amount) & 0xFF;
    pay.amount[1] = ((100 * amount) >> 8) & 0xFF;
    pay.amount[2] = ((100 * amount) >> 16) & 0xFF;

    // Data[15]: 启动/停止
    pay.action = 0x01;  //启动洗车

    // Data[16]: 停止原因
    pay.stopReason = 0;

    // Data[17]: 消费类型
    pay.consumeType = 0x01;  //01 一次性消费  02计量消费

    //Data[18-20]: 消费金额下限(3字节)
    memcpy(pay.consumedMin, pay.amount, 3);

    //Data[21-23]: 消费金额上限(3字节)
    memcpy(pay.consumedMax, pay.amount, 3); 

    //Data[24]: 赠送元素
    pay.time = time;

    terminal_xcj_send_frame(XCJ_CMD_PAY_CONTROL, &pay.memberCode[0], sizeof(XCJ_PAY_CMD_DEF));
}

/**
 * @brief
 * 发送强制控制指令(CMD 0x42) 盒子->洗车机
 * @param action 控制动作 0x01启动 0x02暂停 0x04停止
 */
void xcj_send_force_ctrl(uint8_t action)
{
    uint8_t data[2] = {0};

    data[0] = action; //启动/暂停/停止
    data[1] = 0x00;   //保留

    terminal_xcj_send_frame( XCJ_CMD_FORCE_CTRL, data, sizeof(data));
}

/**
 * @brief
 * 发送设备参数(CMD 0x43) 盒子->洗车机
 * @param param 设备参数
 */
void xcj_send_set_param(TYPE_XCJ_PARAM *param)
{
    //Data区长度: 7个WORD单价(14) + 4个时间(8) + 1等待超时单价(2) + 广告(1) + 温度上下限(2)
    //          + 灯光(1) + 启停设备(1) + 启停营业(1) + IP(4) + 等待时间(1) = 35字节
    uint8_t data[35] = {0};

    if (param == NULL)
        return;

    //Data[2-3]: 泡沫单价
    data[0] = param->foamPrice & 0xFF;
    data[1] = (param->foamPrice >> 8) & 0xFF;

    //Data[4-5]: 蜡水单价
    data[2] = param->waxPrice & 0xFF;
    data[3] = (param->waxPrice >> 8) & 0xFF;

    //Data[6-7]: 清水单价
    data[4] = param->waterPrice & 0xFF;
    data[5] = (param->waterPrice >> 8) & 0xFF;

    //Data[8-9]: 镀膜单价
    data[6] = param->coatPrice & 0xFF;
    data[7] = (param->coatPrice >> 8) & 0xFF;

    //Data[10-11]: 风干单价
    data[8] = param->dryPrice & 0xFF;
    data[9] = (param->dryPrice >> 8) & 0xFF;

    //Data[12-13]: 暖风单价
    data[10] = param->warmPrice & 0xFF;
    data[11] = (param->warmPrice >> 8) & 0xFF;

    //Data[14-15]: 洗车单价
    data[12] = param->washPrice & 0xFF;
    data[13] = (param->washPrice >> 8) & 0xFF;

    //Data[16-17]: 开放时间
    data[14] = param->openTime & 0xFF;
    data[15] = (param->openTime >> 8) & 0xFF;

    //Data[18-19]: 关闭时间
    data[16] = param->closeTime & 0xFF;
    data[17] = (param->closeTime >> 8) & 0xFF;

    //Data[20-21]: 等待时间
    data[18] = param->waitTime & 0xFF;
    data[19] = (param->waitTime >> 8) & 0xFF;

    //Data[22-23]: 超时等待每分钟消费
    data[20] = param->waitTimeoutPrice & 0xFF;
    data[21] = (param->waitTimeoutPrice >> 8) & 0xFF;

    //Data[24]: 广告数量
    data[22] = param->adCount;

    //Data[25]: 温度下限
    data[23] = param->tempMin;

    //Data[26]: 温度上限
    data[24] = param->tempMax;

    //Data[27]: 灯光亮度
    data[25] = param->lightLevel;

    //Data[28]: 启停设备
    data[26] = param->devEnable;

    //Data[29]: 启停营业
    data[27] = param->bizEnable;

    //Data[30-33]: 服务器IP
    memcpy(&data[28], param->serverIp, 4);

    //Data[34]: 洗车等待时间
    data[32] = param->washWaitTimeout;

    //terminal_xcj_send_frame( XCJ_CMD_SET_PARAM, data, sizeof(data));
}

/**
 * @brief
 * 发送获取设备信息指令(CMD 0x44) 盒子->洗车机
 */
void xcj_send_get_info(void)
{
    terminal_xcj_send_frame( XCJ_CMD_GET_INFO, NULL, 0);
}

/**
 * @brief
 * 探虎洗车机串口数据帧处理方法
 * 协议帧格式: 0xBB | Length | Index | CMD | Data[0](sender) | Data[1](reserved) | Data[2..N] | Check | 0xDD
 * @param package 接收到的完整数据帧
 * @param len 数据帧长度
 */
void xcj_packet_parse(uint8_t *package, uint16_t len)
{
    lyy_printf_hex(package, len);
    event_post_log_hextostring(package, len);
    LYY_LOGD("xcj_packet_parse\r\n");

    //帧头帧尾校验
    if (package[0] != XCJ_FRAME_HEAD || package[len - 1] != XCJ_FRAME_END)
    {
        LYY_LOGD("xcj head or end error\r\n");
        return;
    }

    //长度校验: Length字段 = Index + CMD + Data + Check
    //总帧长 = Head(1) + Length(1) + Length字段的值 + End(1) = Length + 3
    uint8_t length = package[1];
    if (len != length + 3)
    {
        LYY_LOGD("xcj length error len:%d expected:%d\r\n", len, length + 3);
        return;
    }

    //校验和
    uint8_t checkCalc = checksum8(package, 1, len - 3);
    if (checkCalc != package[len - 2])
    {
        LYY_LOGD("xcj checksum error calc:%02X recv:%02X\r\n", checkCalc, package[len - 2]);
        return;
    }

    uint8_t index = package[2];   //发送方
    uint8_t cmd = package[3];     //命令字
    //uint8_t sender = package[4];  //Data[0] 发送方标识

    //只处理洗车机发来的数据(Index=0x02标识洗车机主板)
    if (index != XCJ_SENDER_XCJ)
    {
        LYY_LOGD("xcj index not from xcj:%02X\r\n", index);
        return;
    }

    LYY_LOGD("xcj cmd:%02X index:%02X\r\n", cmd, index);

    //Data区从package[6]开始 (Data[2] = package[6])
    uint8_t *data = &package[6];
    uint16_t dataLen = len - 8; //去掉 Head(1)+Length(1)+Index(1)+CMD(1)+Data[0](1)+Data[1](1)+Check(1)+End(1)

    switch (cmd)
    {
    case XCJ_CMD_LOGIN: //0x31 登录命令
        xcj_parse_login(data, dataLen);
        break;

    case XCJ_CMD_CARD_PAY: //0x32 刷卡消费
        xcj_parse_card_pay(data, dataLen);
        break;

    case XCJ_CMD_MEMBER_PAY: //0x33 扫码/APP支付
        xcj_parse_member_pay(data, dataLen);
        break;

    case XCJ_CMD_PAY_CONTROL: //0x41 支付指令应答
        xcj_parse_pay_reply(data, dataLen);
        break;

    case XCJ_CMD_FORCE_CTRL: //0x42 强制控制应答
        xcj_parse_force_reply(data, dataLen);
        break;

    case XCJ_CMD_SET_PARAM: //0x43 参数设置应答
        xcj_parse_set_param_reply(data, dataLen);
        break;

    case XCJ_CMD_GET_INFO: //0x44 获取设备信息应答
        xcj_parse_get_info_reply(data, dataLen);
        break;

    default:
        LYY_LOGD("xcj unknown cmd:%02X\r\n", cmd);
        break;
    }
}
