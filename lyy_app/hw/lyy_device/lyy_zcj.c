/**
 * @file lyy_zcj.c
 * @brief ESP32C2_ZCJ_V1.0.4 业务协议在 BW20 上的 D6 组帧、应答关联和事件分发。
 * @note 同步请求在业务任务中等待；接收回调负责匹配应答并释放对应信号量。
 */
#include "lyy_zcj.h"
#include "task.h"
#include "HAL_Log.h"
#include "HAL_Uart.h"
#include "HAL_Sem.h"
#include "APP_Param.h"
#include "APP_Led.h"
#include "APP_DrillingMode.h"
#include "lyy_device.h"
#include "lyy_tjc.h"
#include "lyy_para.h"
#include "thing_iot.h"
#include "thing_event.h"
#include "thing_property.h"
#include "utils.h"
#include "lyy_start.h"
#include "lyy_mcu_ota.h"

/** @brief 串行化同步请求；不覆盖对端主动上报和无需等待的透传发送。 */
static SemaphoreHandle_t request_lock;
/** @brief 当前同步等待的子命令，0 表示没有正在等待的同步请求。 */
static volatile uint8_t pending_cmd;
static char pending_order[128];
static uint8_t pending_channel;

/** @brief 初始化请求互斥锁；接口使用要求见 lyy_zcj.h。 */
void zcj_protocol_init(void)
{
    if (!request_lock) request_lock = xSemaphoreCreateMutex();
}

SEM_TYPE zcj_sem_param, zcj_sem_pay, zcj_sem_pay_apply, zcj_sem_get_acc;
ZCJ_NOTE_ACCS_TOTAL zcj_acc_t;
ZCJ_PARAM_DEF zcj_param_t;
ZCJ_PARAM_VER_DEF zcj_param_ver_t = {1, "undefine", "undefine"};
uint16_t default_level;
/** @brief 返回模块总账缓存，不发起串口查询。 @return 缓存地址。 */
ZCJ_NOTE_ACCS_TOTAL *zcj_accounts_get(void) { return &zcj_acc_t; }
/** @brief 返回模块纸钞参数缓存。 @return 缓存地址。 */
ZCJ_PARAM_DEF *zcj_param_get(void) { return &zcj_param_t; }
/** @brief 返回模块 MCU 版本缓存。 @return 缓存地址。 */
ZCJ_PARAM_VER_DEF *zcj_param_ver_get(void) { return &zcj_param_ver_t; }
/** @brief 读取最近参数缓存中的极性。 @return level 字段。 */
uint16_t api_pluse_getLevel(void) { return zcj_param_t.level; }

/**
 * @brief 使用主板 UART 发送完整帧，再让出 3 ms。
 * @param[in] buff 已封装的帧数据。
 * @param[in] len 完整帧字节数。
 * @return 固定为 0；底层无发送成功状态返回，不能据此确认 STM32 已接收。
 */
int zcj_terminal_device_data(uint8_t *buff, uint16_t len)
{
    HAL_vUartSend(HAL_UART_MAIN_BOARD, buff, len, 1000);
    vTaskDelay(3 / portTICK_PERIOD_MS);
    return 0;
}
/** @brief 用固定 8 字节普通帧发出配网灯状态。 */
void zcj_notify_provisioning_led(uint8_t status)
{
    uint8_t frame[] = {0xAA, 5, 1, 0xD6, ZCJ_SUB_CMD_PROVISIONING_LED, status, 0, 0xDD};
    frame[6] = check_xor_memory(frame + 1, frame[1]);
    zcj_terminal_device_data(frame, sizeof(frame));
}
/**
 * @brief 按普通帧 LEN 补 AA/INDEX/XOR/DD 并发送。
 * @param[in,out] pack 已设置 len/cmd/sub_cmd/载荷的有效组帧工作区。
 * @return 传输封装返回值，当前固定为 0。
 * @note 调用者保证 LEN 与工作区一致；只适用于普通长度规则。
 */
int8_t zcj_build_packet(PZCJ_PACKET pack)
{
    uint8_t *tp = &pack->head;
    uint16_t len = pack->len + 3;
    pack->head = 0xAA;
    pack->index = 1;
    pack->end = 0xDD;
    pack->check = check_xor_memory(&pack->len, pack->len);
    tp[len - 2] = pack->check;
    tp[len - 1] = pack->end;
    return zcj_terminal_device_data(tp, len);
}
/**
 * @brief 以 D6/01、LEN=FF、小端 u16 长度封装屏幕原始字节。
 * @param[in] buffer 屏幕原始数据，含屏幕自身的命令或分包头。
 * @param[in] len 屏幕原始数据的字节数，上限为 ZCJ_SCREEN_PAYLOAD_MAX。
 * @note 空指针或超长载荷记录错误并返回；发送函数不等待 STM32 或屏幕应答。
 */
void zcj_build_packet_dtu(uint8_t *buffer, uint16_t len)
{
    uint8_t bytes[ZCJ_SCREEN_FRAME_CAPACITY];
    PZCJ_PACKET_DTU tp = (PZCJ_PACKET_DTU)bytes;
    uint16_t length = len + 9;
    if (len > ZCJ_SCREEN_PAYLOAD_MAX || buffer == NULL) {
        LYY_LOGE("Screen DTU payload rejected: %u bytes", (unsigned int)len);
        return;
    }
    tp->head = 0xAA;
    tp->len = 0xFF;
    tp->index = 1;
    tp->cmd = 0xD6;
    tp->sub_cmd = ZCJ_SUB_CMD_DTU_SCREEN;
    tp->data_len = len;
    memcpy(tp->data, buffer, len);
    bytes[length - 2] = check_xor_memory(bytes + 1, length - 3);
    bytes[length - 1] = 0xDD;
    zcj_terminal_device_data(bytes, length);
}
/**
 * @brief 发送带子命令的 D6 载荷，按实际长度计算 XOR。
 * @param[in] sub_cmd D6 子命令。
 * @param[in] pack_data 载荷指针；NULL 时载荷区域保持零值。
 * @param[in] len 载荷字节数，不含子命令和帧开销。
 * @retval -1 载荷超过本地组帧工作区。
 * @retval 0 已调用发送封装，不表示对端业务成功。
 * @note 长载荷令 LEN 饱和为 FF；载荷内部仍须符合具体扩展命令的长度规则。
 */
int zcj_device_uart_send(uint8_t sub_cmd, uint8_t *pack_data, uint16_t len)
{
    uint8_t bytes[1024 + 128] = {0};
    PZCJ_PACKET tp = (PZCJ_PACKET)bytes;
    uint16_t length = len + 7;
    if (len > sizeof(bytes) - 7) return -1;
    tp->head = 0xAA;
    tp->index = 1;
    tp->cmd = 0xD6;
    tp->sub_cmd = sub_cmd;
    tp->len = len + 4 >= 0xFF ? 0xFF : len + 4;
    if (pack_data != NULL && len != 0) memcpy(tp->data, pack_data, len);
    bytes[length - 2] = check_xor_memory(bytes + 1, length - 3);
    bytes[length - 1] = 0xDD;
    return zcj_terminal_device_data(bytes, length);
}
/** @brief 屏幕发送兼容入口，接口契约见 lyy_zcj.h。 */
void zcj_packet_screen_dtu_send(char *pack, uint16_t len)
{ zcj_build_packet_dtu((uint8_t *)pack, len); }
/**
 * @brief 从屏幕扩展帧提取原始数据并同步递交屏幕解析。
 * @param[in,out] packet 完整、已校验的屏幕 D6 帧。
 * @param[in] len 完整帧字节数；不足或内嵌长度超出帧边界时忽略。
 */
void zcj_packet_screen_dtu(uint8_t *packet, uint16_t len)
{
    uint16_t n;
    if (len < 9) return;
    n = (uint16_t)packet[5] | ((uint16_t)packet[6] << 8);
    if (n <= len - 9) tjc_packet_parse(packet + 7, n);
}
/** @brief 类型编号转云端名称，缓冲要求见 lyy_zcj.h。 */
void zcj_note_type_to_string(char *note_string, uint8_t type)
{
    const char *names[] = {"ICT", "ITL", "pulse_coin", "pulse_note", "ST2", "undefine"};
    strcpy(note_string, names[type < 5 ? type : 5]);
}
/** @brief 名称子串匹配类型编号。 @return 0～4 为已知类型，5 为未知。 */
uint8_t zcj_note_type_to_value(char *note_string)
{
    const char *names[] = {"ICT", "ITL", "pulse_coin", "pulse_note", "ST2"};
    if (note_string != NULL)
        for (uint8_t i = 0; i < 5; i++) if (strstr(note_string, names[i])) return i;
    return 5;
}
/**
 * @brief 串行发送同步请求并等待当前命令的应答。
 * @param[in,out] sem 可复用二值信号量句柄；首次使用时创建，等待前清除旧信号。
 * @param[in] cmd 待发送子命令，同时作为接收侧应答关联条件。
 * @param[in] data 对应命令的完整载荷，支付命令另缓存订单/通道用于关联。
 * @param[in] len 载荷字节数。
 * @retval 1 等待期间收到匹配应答信号。
 * @retval 0 锁未初始化、资源分配失败或 2 秒超时。
 * @warning 不得从 UART 接收回调调用，否则会阻塞用于释放信号量的接收任务。
 *          2 秒只限制应答等待，取得请求互斥锁可能等待更久；超时不撤销对端操作。
 */
static int zcj_request(SEM_TYPE *sem, uint8_t cmd, uint8_t *data, uint16_t len)
{
    if (!request_lock || xSemaphoreTake(request_lock, portMAX_DELAY) != pdTRUE) return 0;
    lyy_semNewV2(sem, 0);
    if (*sem == NULL) { xSemaphoreGive(request_lock); return 0; }
    memset(pending_order, 0, sizeof(pending_order));
    if (cmd == ZCJ_SUB_CMD_PAY_APPLY) memcpy(pending_order, data, 128);
    if (cmd == ZCJ_SUB_CMD_PAY_START) {
        ZCJ_PAY_START_REPLY *start = (ZCJ_PAY_START_REPLY *)data;
        memcpy(pending_order, start->order, 128);
        pending_channel = start->channel;
    }
    pending_cmd = cmd;
    zcj_device_uart_send(cmd, data, len);
    int ret = lyy_semWaitTryV2(*sem, 2000);
    pending_cmd = 0;
    xSemaphoreGive(request_lock);
    return ret;
}
/** @brief 同步设置完整纸钞参数，详细返回语义见 lyy_zcj.h。 */
int zcj_cmd_set_param(ZCJ_PARAM_DEF *param)
{ return zcj_request(&zcj_sem_param, ZCJ_SUB_CMD_PARAM_SET, (uint8_t *)param, sizeof(*param)); }
/** @brief 同步查询并更新纸钞参数缓存，详细返回语义见 lyy_zcj.h。 */
int zcj_cmd_get_param(void)
{ return zcj_request(&zcj_sem_param, ZCJ_SUB_CMD_PARAM_GET, NULL, 0); }
/** @brief 以 attribute=0 查询 MCU 版本，详细返回语义见 lyy_zcj.h。 */
int zcj_cmd_get_param_ver(void)
{
    uint8_t attr = 0;
    return zcj_request(&zcj_sem_param, ZCJ_SUB_CMD_DEV_INFOR_UPDATE, &attr, 1);
}
/** @brief 零填充 128 字节订单区后同步申请支付，详细返回语义见 lyy_zcj.h。 */
int zcj_cmd_pay_apply(char *order)
{
    char data[128] = {0};
    if (order == NULL) return 0;
    strncpy(data, order, sizeof(data) - 1);
    return zcj_request(&zcj_sem_pay_apply, ZCJ_SUB_CMD_PAY_APPLY, (uint8_t *)data, sizeof(data));
}
/** @brief 脉冲申请委托通用 D6/05 流程，详细返回语义见 lyy_zcj.h。 */
int zcj_cmd_pay_apply_pulse(char *order) { return zcj_cmd_pay_apply(order); }
/** @brief 同步发起启动接单，不等待最终完成，详细接口见 lyy_zcj.h。 */
int zcj_cmd_pay_start(char *order, uint8_t channel, uint32_t number)
{
    ZCJ_PAY_START_REPLY data = {0};
    if (order == NULL) return 0;
    strncpy(data.order, order, sizeof(data.order) - 1);
    data.channel = channel;
    data.number = number;
    return zcj_request(&zcj_sem_pay, ZCJ_SUB_CMD_PAY_START, (uint8_t *)&data, sizeof(data));
}
/** @brief 发送无需等待的启动命令，详细接口见 lyy_zcj.h。 */
void zcj_cmd_pay_start_pulse(char *order, uint8_t channel, uint32_t number)
{
    ZCJ_PAY_START_REPLY data = {0};
    if (order == NULL) return;
    strncpy(data.order, order, sizeof(data.order) - 1);
    data.channel = channel;
    data.number = number;
    zcj_device_uart_send(ZCJ_SUB_CMD_PAY_START, (uint8_t *)&data, sizeof(data));
}
/**
 * @brief 参数设置 ACK 仅在当前等待命令为 02 时唤醒请求。
 * @param[in] packet 完整帧，当前不读取其载荷。
 * @param[in] len 完整帧字节数，至少 7 字节。
 */
void zcj_packet_reply_param_set(uint8_t *packet, uint16_t len)
{
    (void)packet;
    if (len >= 7 && pending_cmd == ZCJ_SUB_CMD_PARAM_SET) lyy_semSignalV2(zcj_sem_param);
}
/**
 * @brief 接收完整参数缓存，当前等待 03 时唤醒请求。
 * @param[in] packet 完整 D6/03 帧。
 * @param[in] len 完整帧字节数，必须覆盖参数结构及 7 字节开销。
 */
void zcj_packet_reply_param_get(uint8_t *packet, uint16_t len)
{
    if (len < sizeof(zcj_param_t) + 7) return;
    memcpy(&zcj_param_t, packet + 5, sizeof(zcj_param_t));
    if (pending_cmd == ZCJ_SUB_CMD_PARAM_GET) lyy_semSignalV2(zcj_sem_param);
}
/**
 * @brief 匹配 D6/05 订单，保存申请状态并唤醒当前请求。
 * @param[in] packet 完整帧，订单从字节 6 开始，比较完整 128 字节。
 * @param[in] len 完整帧字节数，短帧或非当前订单忽略。
 */
void zcj_packet_reply_pay_apply(uint8_t *packet, uint16_t len)
{
    if (len < sizeof(ZCJ_PAY_APPLY_REPLY) + 7 || pending_cmd != ZCJ_SUB_CMD_PAY_APPLY ||
        memcmp(packet + 6, pending_order, 128)) return;
    g_xlyyHwMark.applyResult = packet[5];
    lyy_semSignalV2(zcj_sem_pay_apply);
}
/**
 * @brief 按 channel/u32 result/order[128] 解析 06 即时启动应答。
 * @param[in] packet 完整 140 字节帧，订单从字节 10 开始。
 * @param[in] len 完整帧字节数。
 * @note 只在当前等待启动且通道/订单一致时唤醒；不能用 07 的 u8 结果布局代替。
 */
void zcj_packet_reply_pay_start(uint8_t *packet, uint16_t len)
{
    /* STM32 SG0B1 sends channel/u32 result/order; final 07 uses u8 result. */
    if (len != 140 || pending_cmd != ZCJ_SUB_CMD_PAY_START ||
        packet[5] != pending_channel || memcmp(packet + 10, pending_order, 128)) return;
    uint32_t result;
    memcpy(&result, packet + 6, sizeof(result));
    g_xlyyHwMark.startResult = (int)result;
    lyy_semSignalV2(zcj_sem_pay);
}
/**
 * @brief 回显 ACK 给 STM32，并将 07 最终结果转换为云端 start_result 事件。
 * @param[in,out] packet 完整帧，发送 ACK 后会给订单最后一个字节写 NUL。
 * @param[in] len 完整帧字节数。
 * @note 此处理不唤醒 06 即时接单请求，二者是不同业务阶段。
 */
void zcj_packet_cmd_pay_start_result(uint8_t *packet, uint16_t len)
{
    PZCJ_PAY_START_RESULT_REPLY reply = (PZCJ_PAY_START_RESULT_REPLY)(packet + 5);
    if (len < sizeof(*reply) + 7) return;
    zcj_device_uart_send(ZCJ_SUB_CMD_PAY_RESULT_UPDATE, &reply->channel, sizeof(*reply));
    reply->order[sizeof(reply->order) - 1] = 0;
    event_post_start_result_note(reply->result, reply->channel, reply->order);
}
/**
 * @brief 对每个合法现金上报先 ACK，再按上一笔 8 位流水号去重并发云事件。
 * @param[in] packet 完整 D6/08 帧，载荷为 unique/channel/inc/total。
 * @param[in] len 完整帧字节数。
 * @note NOTE 模式发 accounts，其他模式发 accounts_pulse_coin；去重仅在本次运行内有效，
 *       不保证乱序、跨重启或断网事件逐笔补发，ACK 不代表云端已入账。
 */
void zcj_packet_cmd_accounts(uint8_t *packet, uint16_t len)
{
    static uint8_t unique_flag, have_unique;
    PZCJ_ACCOUNTS_UPDATE reply = (PZCJ_ACCOUNTS_UPDATE)(packet + 5);
    uint8_t ack[3];
    if (len < sizeof(*reply) + 7) return;
    ack[0] = reply->unique; ack[1] = reply->channel; ack[2] = 1;
    zcj_device_uart_send(ZCJ_SUB_CMD_DEV_ACCOUNTS_UPDATE, ack, sizeof(ack));
    if (have_unique && unique_flag == reply->unique) return;
    unique_flag = reply->unique;
    have_unique = 1;
    if (get_dev_type_code() == DEVICE_TYPE_UART_NOTE)
        event_post_account_note(reply->inc, reply->total, "offline_pay", NULL, "accounts", reply->channel);
    else
        event_post_account_pulse(reply->inc, reply->total, "offline_pay", NULL, "accounts_pulse_coin");
}
/**
 * @brief 确认对端状态通知并记录状态/错误码，不生成独立云故障事件。
 * @param[in] packet 完整帧，字节 5/6 为状态与错误码。
 * @param[in] len 完整帧字节数，至少 9 字节。
 */
void zcj_packet_cmd_status(uint8_t *packet, uint16_t len)
{
    uint8_t result = 1;
    if (len < 9) return;
    zcj_device_uart_send(ZCJ_SUB_CMD_DEV_STATUS_UPDATE, &result, 1);
    LYY_LOGD("status:%d code:%d", packet[5], packet[6]);
}
/**
 * @brief 更新版本缓存；主动上报时 ACK 并上报云端，查询应答时唤醒等待请求。
 * @param[in] packet 完整 D6/10 帧。
 * @param[in] len 完整帧字节数，必须包含整个版本结构。
 */
void zcj_packet_cmd_dev_infor(uint8_t *packet, uint16_t len)
{
    uint8_t result = 1;
    if (len < sizeof(zcj_param_ver_t) + 7) return;
    memcpy(&zcj_param_ver_t, packet + 5, sizeof(zcj_param_ver_t));
    zcj_param_ver_t.ver_soft[63] = 0;
    zcj_param_ver_t.ver_hard[63] = 0;
    if (zcj_param_ver_t.attribute == 1)
    {
        zcj_device_uart_send(ZCJ_SUB_CMD_DEV_INFOR_UPDATE, &result, 1);
        thing_ota_inform_version(zcj_param_ver_t.ver_soft, "zcjBoard");
        vTaskDelay(pdMS_TO_TICKS(50));
        property_post_dev_mcu_version(zcj_param_ver_t.ver_soft, zcj_param_ver_t.ver_hard);
    }
    else if (pending_cmd == ZCJ_SUB_CMD_DEV_INFOR_UPDATE) lyy_semSignalV2(zcj_sem_param);
}
/**
 * @brief 仅记录设备类型设置的返回字节。
 * @param[in] packet 完整 D6/0A 帧。
 * @param[in] len 完整帧字节数。
 */
void zcj_packet_reply_dev_type_set(uint8_t *packet, uint16_t len)
{
    if (len >= 8) LYY_LOGD("set device type result:%d", packet[5]);
}
/**
 * @brief 回复 STM32 的 0B 配置/存活轮询，包括机台速率、灯状态和输出端口。
 * @param[in] packet 轮询帧，当前不读取载荷。
 * @param[in] len 完整帧字节数，当前未使用。
 * @note 模块间速率仍固定 115200；这里的 brt 是对端外设参数，极性来自参数缓存。
 */
void zcj_packet_cmd_dev_type_get(uint8_t *packet, uint16_t len)
{
    ZCJ_DEV_TYPE_INFOR info = {0};
    APP_PULSE_T *pulse = APP_pxlGetParamePulse();
    (void)packet; (void)len;
    info.dev_type = get_dev_type_code();
    info.brt = APP_ulGetParamBaudRate();
    info.led_status = get_led_status();
    info.pulse_width = pulse->ulPulseWidth;
    info.pulse_space = pulse->ulPulseSpace;
    info.pluse_level = api_pluse_getLevel();
    info.pluse_output = ulPulsetype.bytes[0] == START_TYPE_LEVEL ? 1 : 0;
    info.pluse_outputPort = ulPulsetype.bytes[1];
    zcj_device_uart_send(ZCJ_SUB_CMD_DEV_TYPE_GET, (uint8_t *)&info, sizeof(info));
}
/** @brief 启动轮询用非阻塞总账请求，存在同步请求时跳过本次发送。 */
void zcj_request_total_acc(void)
{
    if (!pending_cmd) zcj_device_uart_send(ZCJ_SUB_CMD_DEV_TOTAL_ACCOUNTS, NULL, 0);
}
/** @brief 同步总账请求，返回含义见 lyy_zcj.h。 */
int zcj_packet_cmd_total_acc_get(void)
{ return zcj_request(&zcj_sem_get_acc, ZCJ_SUB_CMD_DEV_TOTAL_ACCOUNTS, NULL, 0); }
/**
 * @brief 校验通道项数及载荷长度后整体更新总账缓存。
 * @param[in] packet 完整 D6/0C 帧，字节 5 为项数。
 * @param[in] len 完整帧字节数，载荷必须恰为 1+4*项数。
 * @note 即使没有同步等待也更新缓存，以支持非阻塞启动查询。
 */
void zcj_packet_reply_total_acc_get(uint8_t *packet, uint16_t len)
{
    uint16_t n;
    if (len < 8) return;
    n = len - 7;
    if (n > sizeof(zcj_acc_t) || packet[5] > 12 || n != 1 + packet[5] * 4) return;
    memset(&zcj_acc_t, 0, sizeof(zcj_acc_t));
    memcpy(&zcj_acc_t, packet + 5, n);
    if (zcj_acc_t.ch_sum > 12) zcj_acc_t.ch_sum = 12;
    if (pending_cmd == ZCJ_SUB_CMD_DEV_TOTAL_ACCOUNTS) lyy_semSignalV2(zcj_sem_get_acc);
}
/** @brief 构造 name/total/offset/length/data 升级块并发送，详细接口见 lyy_zcj.h。 */
void zcj_packet_dev_upgrade(char *name, uint32_t total, uint32_t offset, char *data, uint32_t len)
{
    ZJC_OPEN_UPDATE_DATA_T *p;
    uint8_t *bytes;
    if (name == NULL || data == NULL || len > 1024) return;
    bytes = pvPortMalloc(1024 + 128);
    if (bytes == NULL) return;
    memset(bytes, 0, 1024 + 128);
    p = (ZJC_OPEN_UPDATE_DATA_T *)bytes;
    if (strlen(name) < sizeof(p->fileName)) strcpy(p->fileName, name);
    p->fileTotal = total; p->fileOffset = offset; p->upDataLen = len;
    memcpy(&p->upData, data, len);
    zcj_device_uart_send(ZCJ_SUB_CMD_DEV_UPGRADE, bytes, sizeof(*p) + len - 1);
    vPortFree(bytes);
}
/**
 * @brief 将升级 ACK 递交分包发送状态机作完整字段匹配。
 * @param[in] packet 完整 D6/0E 应答，从字节 4 开始包含子命令结构。
 * @param[in] len 完整帧字节数。
 */
void zcj_packet_reply_upgrade(uint8_t *packet, uint16_t len)
{
    ZJC_OPEN_UPDATE_RES_T *p = (ZJC_OPEN_UPDATE_RES_T *)(packet + 4);
    if (len < sizeof(*p) + 6) return;
    LYY_LOGD("zcj upgrade offset:%lu", (unsigned long)p->fileOffset);
    lyy_mcu_ota_ack(packet, len);
}
/** @brief 已校验帧的 D6 子命令分发入口，数据要求见 lyy_zcj.h。 */
void lyy_zcj_packet_parse(uint8_t *packet, uint16_t len)
{
    if (packet == NULL || len < 7) return;
    switch (packet[4])
    {
    case ZCJ_SUB_CMD_DTU_SCREEN: zcj_packet_screen_dtu(packet, len); break;
    case ZCJ_SUB_CMD_PARAM_SET: zcj_packet_reply_param_set(packet, len); break;
    case ZCJ_SUB_CMD_PARAM_GET: zcj_packet_reply_param_get(packet, len); break;
    case ZCJ_SUB_CMD_PAY_APPLY: zcj_packet_reply_pay_apply(packet, len); break;
    case ZCJ_SUB_CMD_PAY_START: zcj_packet_reply_pay_start(packet, len); break;
    case ZCJ_SUB_CMD_PAY_RESULT_UPDATE: zcj_packet_cmd_pay_start_result(packet, len); break;
    case ZCJ_SUB_CMD_DEV_ACCOUNTS_UPDATE: zcj_packet_cmd_accounts(packet, len); break;
    case ZCJ_SUB_CMD_DEV_STATUS_UPDATE: zcj_packet_cmd_status(packet, len); break;
    case ZCJ_SUB_CMD_DEV_INFOR_UPDATE: zcj_packet_cmd_dev_infor(packet, len); break;
    case ZCJ_SUB_CMD_DEV_TYPE_SET: zcj_packet_reply_dev_type_set(packet, len); break;
    case ZCJ_SUB_CMD_DEV_TYPE_GET: zcj_packet_cmd_dev_type_get(packet, len); break;
    case ZCJ_SUB_CMD_DEV_TOTAL_ACCOUNTS: zcj_packet_reply_total_acc_get(packet, len); break;
    case ZCJ_SUB_CMD_DEV_UPGRADE: zcj_packet_reply_upgrade(packet, len); break;
    default: break;
    }
}
