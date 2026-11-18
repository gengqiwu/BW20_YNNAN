/**
 * @file lyy_zcj.h
 * @brief STM32 纸钞盒 D6 协议、载荷布局与同步业务接口。
 * @details 普通帧为 AA LEN INDEX D6 SUBCMD PAYLOAD XOR DD；载荷 n 字节时
 *          LEN=n+4、完整帧长=n+7。多字节整数使用小端，packed 结构与对端逐字节对应。
 * @warning 不得为对齐而插入字段或改字段类型；屏幕扩展帧和升级扩展帧长度规则不同。
 */
#ifndef __LYY_ZCJ_H__
#define __LYY_ZCJ_H__
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "HAL_System.h"
#include "FreeRTOS.h"
#include "semphr.h"
#define SEM_TYPE SemaphoreHandle_t

/** @brief 特殊现金通道：100 表示纸钞脉冲，101 表示硬币脉冲。 */
enum { NOTE_CHANNLE_PULSE = 100, NOTE_CHANNLE_COINS = 101 };
/** @brief D6 子命令编号，必须与 STM32 固件保持一致。 */
enum {
    ZCJ_SUB_CMD_DEFAULT = 0,
    ZCJ_SUB_CMD_DTU_SCREEN = 1,
    ZCJ_SUB_CMD_PARAM_SET = 2,
    ZCJ_SUB_CMD_PARAM_GET = 3,
    ZCJ_SUB_CMD_PAY_APPLY = 5,
    ZCJ_SUB_CMD_PAY_START = 6,
    ZCJ_SUB_CMD_PAY_RESULT_UPDATE = 7,
    ZCJ_SUB_CMD_DEV_ACCOUNTS_UPDATE = 8,
    ZCJ_SUB_CMD_DEV_STATUS_UPDATE = 9,
    ZCJ_SUB_CMD_DEV_TYPE_SET = 0x0A,
    ZCJ_SUB_CMD_DEV_TYPE_GET = 0x0B,
    ZCJ_SUB_CMD_DEV_TOTAL_ACCOUNTS = 0x0C,
    ZCJ_SUB_CMD_DEV_UPGRADE = 0x0E,
    ZCJ_SUB_CMD_DEV_INFOR_UPDATE = 0x10
};
/** @brief 脉冲空闲极性枚举：NO/NC 对应对端保存的 0/1。 */
typedef enum { PULSE_EXPORT_NO, PULSE_EXPORT_NC } PULSE_EXPORT_E;
/**
 * @brief 普通 D6 组帧工作区。
 * @note data 为最大工作区；实际 XOR/DD 紧跟有效载荷，由组帧函数按 LEN 覆盖写入。
 */
typedef struct {
    uint8_t head, len, index, cmd, sub_cmd; /**< AA、协议长度、索引、D6 和子命令。 */
    uint8_t data[1024 + 128];              /**< 载荷工作区，不代表普通帧 LEN 可容纳全部容量。 */
    uint8_t check, end;                    /**< 工作区字段，实际校验/帧尾位置由有效长度决定。 */
} __attribute__((aligned(1), packed)) ZCJ_PACKET, *PZCJ_PACKET;
/** @brief D6/01 屏幕透传扩展帧，LEN=FF，完整帧长=data_len+9。 */
typedef struct {
    uint8_t head, len, index, cmd, sub_cmd;
    uint16_t data_len; /**< 小端屏幕原始数据长度，不含封装开销。 */
    uint8_t data[1024 + 128];
    uint8_t check, end;
} __attribute__((aligned(1), packed)) ZCJ_PACKET_DTU, *PZCJ_PACKET_DTU;
/**
 * @brief D6/02 写参数与 D6/03 读参数应答共用的 11 字节载荷。
 * @note note_type 为 ICT/ITL/pulse_coin/pulse_note/ST2；width/space 单位 ms，
 *       level 为 NO/NC，IctStartMode=1 表示 A、0 表示 B，res 保留对端字节。
 */
typedef struct {
    uint8_t note_type;
    uint16_t width, space;
    uint8_t level, OutMode, IctStartMode, res[3];
} __attribute__((aligned(1), packed)) ZCJ_PARAM_DEF, *PZCJ_PARAM_DEF;
/** @brief D6/10 版本信息；attribute=0 表示查询应答，1 表示对端主动上报。 */
typedef struct {
    uint8_t attribute;
    char ver_soft[64], ver_hard[64];
} __attribute__((aligned(1), packed)) ZCJ_PARAM_VER_DEF, *PZCJ_PARAM_VER_DEF;
/** @brief D6/05 支付申请应答：状态字节及原样回传的 128 字节订单。 */
typedef struct {
    uint8_t status, order[128];
} __attribute__((aligned(1), packed)) ZCJ_PAY_APPLY_REPLY, *PZCJ_PAY_APPLY_REPLY;
/**
 * @brief D6/06 支付启动请求载荷，不是启动应答。
 * @note channel 为纸钞通道或特殊脉冲通道，number 为数量；电平模式由 STM32 解释单位。
 */
typedef struct {
    uint8_t channel;
    uint32_t number;
    char order[128];
} __attribute__((aligned(1), packed)) ZCJ_PAY_START_REPLY, *PZCJ_PAY_START_REPLY;
/**
 * @brief D6/07 最终结果载荷：channel + u8 result + order[128]。
 * @warning D6/06 即时接单应答使用 u32 result，不能用此结构直接解析；完整应答长 140 字节。
 */
typedef struct {
    uint8_t channel, result;
    char order[128];
} __attribute__((aligned(1), packed)) ZCJ_PAY_START_RESULT_REPLY, *PZCJ_PAY_START_RESULT_REPLY;
/** @brief D6/08 现金账目上报：8 位流水号、通道、增量与该通道累计值。 */
typedef struct {
    uint8_t unique, channel;
    uint32_t inc, total;
} __attribute__((aligned(1), packed)) ZCJ_ACCOUNTS_UPDATE, *PZCJ_ACCOUNTS_UPDATE;
/**
 * @brief D6/0B 配置轮询应答，设备类型/机台速率/灯状态/脉冲参数共 21 字节。
 * @note brt 仅是 STM32 外设速率；pluse_output=0/1 为脉冲/电平，
 *       pluse_outputPort=0/1/2 为 COIN/ZCJ1/ZCJ2，array 为保留字节。
 */
typedef struct {
    uint32_t dev_type, brt;
    uint8_t led_status;
    uint16_t pulse_width, pulse_space;
    uint8_t pluse_level, pluse_output, pluse_outputPort, array[5];
} __attribute__((aligned(1), packed)) ZCJ_DEV_TYPE_INFOR, *PZCJ_DEV_TYPE_INFOR;
/**
 * @brief D6/0C 总账缓存，ch_sum 表示后续有效 u32 项数，最大 12。
 * @note 接收实际载荷长为 1+4*ch_sum；现金云事件另按参考版规则映射各通道。
 */
typedef struct {
    uint8_t ch_sum;
    uint32_t ch_acc[12];
} __attribute__((aligned(1), packed)) ZCJ_NOTE_ACCS_TOTAL, *PZCJ_NOTE_ACCS_TOTAL;
/**
 * @brief D6/0E 升级请求头与变长固件数据首字节。
 * @note fileTotal/fileOffset/upDataLen 单位均为字节；upData 是变长数据起点而非单字节块。
 */
typedef struct {
    char fileName[20];
    uint32_t fileTotal, fileOffset, upDataLen;
    char upData;
} __attribute__((aligned(1), packed)) ZJC_OPEN_UPDATE_DATA_T;
/** @brief D6/0E 应答从子命令开始的 34 字节结构；result=0 才表示该块成功。 */
typedef struct {
    uint8_t subCmd;
    char fileName[20];
    uint32_t fileTotal, fileOffset, upDataLen;
    char result;
} __attribute__((aligned(1), packed)) ZJC_OPEN_UPDATE_RES_T;

/** @brief 初始化同步请求互斥锁，应在注册 UART 业务接收回调前调用。 */
void zcj_protocol_init(void);
/**
 * @brief 分发已完成校验的 D6 帧。
 * @param[in,out] packet 含 AA/XOR/DD 的完整可写帧；07 处理会补订单终止符。
 * @param[in] len 完整帧字节数，不能传载荷长度。
 * @pre 调用方已进行长度累积、帧头/帧尾和 XOR 校验。
 */
void lyy_zcj_packet_parse(uint8_t *packet, uint16_t len);
/**
 * @brief 同步写入 D6/02 纸钞参数，最多等待应答 2 秒。
 * @param[in] param 完整参数结构，不可为 NULL。
 * @retval 1 收到对应参数写入 ACK。
 * @retval 0 初始化/同步资源失败或等待超时；不代表对端一定未执行写入。
 */
int zcj_cmd_set_param(ZCJ_PARAM_DEF *param);
/** @brief 获取最近收到的纸钞参数缓存。 @return 模块持有的可写缓存，不执行串口查询。 */
ZCJ_PARAM_DEF *zcj_param_get(void);
/**
 * @brief 将纸钞类型编号转换为云端名称。
 * @param[out] note_string 调用方提供的字符串缓冲，至少 11 字节。
 * @param[in] type 类型编号 0～4，其他值输出 undefine。
 */
void zcj_note_type_to_string(char *note_string, uint8_t type);
/**
 * @brief 识别类型名称中的 ICT/ITL/pulse_coin/pulse_note/ST2 子串。
 * @param[in] note_string NUL 结尾名称，可为 NULL。
 * @return 0～4 为匹配类型；5 表示 NULL 或未匹配。
 */
uint8_t zcj_note_type_to_value(char *note_string);
/**
 * @brief 发送 D6/06 并同步等待当前订单/通道的接单应答。
 * @param[in] order NUL 结尾订单，最多复制 127 字节，NULL 时返回 0。
 * @param[in] channel 对端通道编号。
 * @param[in] number 启动数量，具体单位由 STM32 的设备模式决定。
 * @retval 1 收到匹配应答，业务结果查看 g_xlyyHwMark.startResult。
 * @retval 0 请求资源失败或 2 秒未收到匹配应答。
 * @note 返回 1 只表示收到接单应答，最终完成另由 D6/07 上报。
 */
int zcj_cmd_pay_start(char *order, uint8_t channel, uint32_t number);
/**
 * @brief 发送 D6/05 并等待匹配订单的支付申请应答。
 * @param[in] order NUL 结尾订单，最多复制 127 字节，NULL 时返回 0。
 * @retval 1 收到匹配应答，业务状态查看 g_xlyyHwMark.applyResult。
 * @retval 0 资源失败或 2 秒等待超时。
 */
int zcj_cmd_pay_apply(char *order);
/**
 * @brief 脉冲业务兼容入口，委托 zcj_cmd_pay_apply()。
 * @param[in] order NUL 结尾订单。
 * @return 与 zcj_cmd_pay_apply() 相同。
 */
int zcj_cmd_pay_apply_pulse(char *order);
/**
 * @brief 直接发送 D6/06，不建立等待请求或检查即时接单应答。
 * @param[in] order NUL 结尾订单，NULL 时不发送。
 * @param[in] channel 对端输出通道。
 * @param[in] number 启动数量。
 * @note 需要即时返回值的云端启动流程应使用 zcj_cmd_pay_start()。
 */
void zcj_cmd_pay_start_pulse(char *order, uint8_t channel, uint32_t number);
/** @brief 同步发送 D6/03 更新参数缓存。 @retval 1 收到参数。 @retval 0 资源失败或超时。 */
int zcj_cmd_get_param(void);
/**
 * @brief 经 D6/01 将屏幕数据透传给 STM32，不直接使用 BW20 屏幕串口。
 * @param[in] pack 屏幕原始字节，可包含 NUL，不能为 NULL。
 * @param[in] len 原始字节数，超过工作区上限时忽略。
 */
void zcj_packet_screen_dtu_send(char *pack, uint16_t len);
/** @brief 查询 D6/10 MCU 版本并更新缓存。 @retval 1 收到查询应答。 @retval 0 资源失败或超时。 */
int zcj_cmd_get_param_ver(void);
/** @brief 获取最近的 MCU 版本缓存。 @return 模块拥有的可写缓存指针，不主动查询。 */
ZCJ_PARAM_VER_DEF *zcj_param_ver_get(void);
/** @brief 获取 STM32 总账缓存。 @return 模块拥有的可写缓存，ch_sum=0 表示尚无有效总账。 */
ZCJ_NOTE_ACCS_TOTAL *zcj_accounts_get(void);
/** @brief 同步查询 D6/0C 总账。 @retval 1 收到有效总账。 @retval 0 资源失败或超时。 */
int zcj_packet_cmd_total_acc_get(void);
/** @brief 非阻塞发送 D6/0C 查询；存在同步请求时跳过，启动定时轮询使用此入口。 */
void zcj_request_total_acc(void);
/** @brief 获取最近参数缓存中的脉冲极性。 @return level 字段，不是 PA26 配网模式电平。 */
uint16_t api_pluse_getLevel(void);
/**
 * @brief 发送一个 D6/0E 升级数据块，仅组帧，不等待 ACK。
 * @param[in] pcFileName NUL 结尾名称，少于 20 字节才会写入报文名称字段。
 * @param[in] fileTotal 镜像总字节数。
 * @param[in] ulOffset 当前块在镜像中的字节偏移。
 * @param[in] pcUpData 当前块数据，不可为 NULL。
 * @param[in] ulUpDataLen 当前块长度，最大 1024 字节。
 * @note NULL 输入、超长块或内存分配失败时不发送；等待与超时由 lyy_mcu_ota 模块管理。
 */
void zcj_packet_dev_upgrade(char *pcFileName, uint32_t fileTotal, uint32_t ulOffset,
                            char *pcUpData, uint32_t ulUpDataLen);
#endif
