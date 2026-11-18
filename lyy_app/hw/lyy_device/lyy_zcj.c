/* ESP32C2_ZCJ_V1.0.4 business protocol; BW transport/RTOS bindings. */
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

static SemaphoreHandle_t request_lock;
static volatile uint8_t pending_cmd;
static char pending_order[128];
static uint8_t pending_channel;

void zcj_protocol_init(void)
{
    if (!request_lock) request_lock = xSemaphoreCreateMutex();
}

SEM_TYPE zcj_sem_param, zcj_sem_pay, zcj_sem_pay_apply, zcj_sem_get_acc;
ZCJ_NOTE_ACCS_TOTAL zcj_acc_t;
ZCJ_PARAM_DEF zcj_param_t;
ZCJ_PARAM_VER_DEF zcj_param_ver_t = {1, "undefine", "undefine"};
uint16_t default_level;
ZCJ_NOTE_ACCS_TOTAL *zcj_accounts_get(void) { return &zcj_acc_t; }
ZCJ_PARAM_DEF *zcj_param_get(void) { return &zcj_param_t; }
ZCJ_PARAM_VER_DEF *zcj_param_ver_get(void) { return &zcj_param_ver_t; }
uint16_t api_pluse_getLevel(void) { return zcj_param_t.level; }

int zcj_terminal_device_data(uint8_t *buff, uint16_t len)
{
    HAL_vUartSend(HAL_UART_MAIN_BOARD, buff, len, 1000);
    vTaskDelay(3 / portTICK_PERIOD_MS);
    return 0;
}
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
void zcj_build_packet_dtu(uint8_t *buffer, uint16_t len)
{
    uint8_t bytes[1024 + 128] = {0};
    PZCJ_PACKET_DTU tp = (PZCJ_PACKET_DTU)bytes;
    uint16_t length = len + 9;
    if (len > sizeof(bytes) - 9 || buffer == NULL) return;
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
void zcj_packet_screen_dtu_send(char *pack, uint16_t len)
{ zcj_build_packet_dtu((uint8_t *)pack, len); }
void zcj_packet_screen_dtu(uint8_t *packet, uint16_t len)
{
    uint16_t n;
    if (len < 9) return;
    n = (uint16_t)packet[5] | ((uint16_t)packet[6] << 8);
    if (n <= len - 9) tjc_packet_parse(packet + 7, n);
}
void zcj_note_type_to_string(char *note_string, uint8_t type)
{
    const char *names[] = {"ICT", "ITL", "pulse_coin", "pulse_note", "ST2", "undefine"};
    strcpy(note_string, names[type < 5 ? type : 5]);
}
uint8_t zcj_note_type_to_value(char *note_string)
{
    const char *names[] = {"ICT", "ITL", "pulse_coin", "pulse_note", "ST2"};
    if (note_string != NULL)
        for (uint8_t i = 0; i < 5; i++) if (strstr(note_string, names[i])) return i;
    return 5;
}
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
int zcj_cmd_set_param(ZCJ_PARAM_DEF *param)
{ return zcj_request(&zcj_sem_param, ZCJ_SUB_CMD_PARAM_SET, (uint8_t *)param, sizeof(*param)); }
int zcj_cmd_get_param(void)
{ return zcj_request(&zcj_sem_param, ZCJ_SUB_CMD_PARAM_GET, NULL, 0); }
int zcj_cmd_get_param_ver(void)
{
    uint8_t attr = 0;
    return zcj_request(&zcj_sem_param, ZCJ_SUB_CMD_DEV_INFOR_UPDATE, &attr, 1);
}
int zcj_cmd_pay_apply(char *order)
{
    char data[128] = {0};
    if (order == NULL) return 0;
    strncpy(data, order, sizeof(data) - 1);
    return zcj_request(&zcj_sem_pay_apply, ZCJ_SUB_CMD_PAY_APPLY, (uint8_t *)data, sizeof(data));
}
int zcj_cmd_pay_apply_pulse(char *order) { return zcj_cmd_pay_apply(order); }
int zcj_cmd_pay_start(char *order, uint8_t channel, uint32_t number)
{
    ZCJ_PAY_START_REPLY data = {0};
    if (order == NULL) return 0;
    strncpy(data.order, order, sizeof(data.order) - 1);
    data.channel = channel;
    data.number = number;
    return zcj_request(&zcj_sem_pay, ZCJ_SUB_CMD_PAY_START, (uint8_t *)&data, sizeof(data));
}
void zcj_cmd_pay_start_pulse(char *order, uint8_t channel, uint32_t number)
{
    ZCJ_PAY_START_REPLY data = {0};
    if (order == NULL) return;
    strncpy(data.order, order, sizeof(data.order) - 1);
    data.channel = channel;
    data.number = number;
    zcj_device_uart_send(ZCJ_SUB_CMD_PAY_START, (uint8_t *)&data, sizeof(data));
}
void zcj_packet_reply_param_set(uint8_t *packet, uint16_t len)
{
    (void)packet;
    if (len >= 7 && pending_cmd == ZCJ_SUB_CMD_PARAM_SET) lyy_semSignalV2(zcj_sem_param);
}
void zcj_packet_reply_param_get(uint8_t *packet, uint16_t len)
{
    if (len < sizeof(zcj_param_t) + 7) return;
    memcpy(&zcj_param_t, packet + 5, sizeof(zcj_param_t));
    if (pending_cmd == ZCJ_SUB_CMD_PARAM_GET) lyy_semSignalV2(zcj_sem_param);
}
void zcj_packet_reply_pay_apply(uint8_t *packet, uint16_t len)
{
    if (len < sizeof(ZCJ_PAY_APPLY_REPLY) + 7 || pending_cmd != ZCJ_SUB_CMD_PAY_APPLY ||
        memcmp(packet + 6, pending_order, 128)) return;
    g_xlyyHwMark.applyResult = packet[5];
    lyy_semSignalV2(zcj_sem_pay_apply);
}
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
void zcj_packet_cmd_pay_start_result(uint8_t *packet, uint16_t len)
{
    PZCJ_PAY_START_RESULT_REPLY reply = (PZCJ_PAY_START_RESULT_REPLY)(packet + 5);
    if (len < sizeof(*reply) + 7) return;
    zcj_device_uart_send(ZCJ_SUB_CMD_PAY_RESULT_UPDATE, &reply->channel, sizeof(*reply));
    reply->order[sizeof(reply->order) - 1] = 0;
    event_post_start_result_note(reply->result, reply->channel, reply->order);
}
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
void zcj_packet_cmd_status(uint8_t *packet, uint16_t len)
{
    uint8_t result = 1;
    if (len < 9) return;
    zcj_device_uart_send(ZCJ_SUB_CMD_DEV_STATUS_UPDATE, &result, 1);
    LYY_LOGD("status:%d code:%d", packet[5], packet[6]);
}
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
void zcj_packet_reply_dev_type_set(uint8_t *packet, uint16_t len)
{
    if (len >= 8) LYY_LOGD("set device type result:%d", packet[5]);
}
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
void zcj_request_total_acc(void)
{
    if (!pending_cmd) zcj_device_uart_send(ZCJ_SUB_CMD_DEV_TOTAL_ACCOUNTS, NULL, 0);
}
int zcj_packet_cmd_total_acc_get(void)
{ return zcj_request(&zcj_sem_get_acc, ZCJ_SUB_CMD_DEV_TOTAL_ACCOUNTS, NULL, 0); }
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
void zcj_packet_reply_upgrade(uint8_t *packet, uint16_t len)
{
    ZJC_OPEN_UPDATE_RES_T *p = (ZJC_OPEN_UPDATE_RES_T *)(packet + 4);
    if (len < sizeof(*p) + 6) return;
    LYY_LOGD("zcj upgrade offset:%lu", (unsigned long)p->fileOffset);
    lyy_mcu_ota_ack(packet, len);
}
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
