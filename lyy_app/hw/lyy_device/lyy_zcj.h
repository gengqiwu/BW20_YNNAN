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

enum { NOTE_CHANNLE_PULSE = 100, NOTE_CHANNLE_COINS = 101 };
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
typedef enum { PULSE_EXPORT_NO, PULSE_EXPORT_NC } PULSE_EXPORT_E;
typedef struct {
    uint8_t head, len, index, cmd, sub_cmd;
    uint8_t data[1024 + 128];
    uint8_t check, end;
} __attribute__((aligned(1), packed)) ZCJ_PACKET, *PZCJ_PACKET;
typedef struct {
    uint8_t head, len, index, cmd, sub_cmd;
    uint16_t data_len;
    uint8_t data[1024 + 128];
    uint8_t check, end;
} __attribute__((aligned(1), packed)) ZCJ_PACKET_DTU, *PZCJ_PACKET_DTU;
typedef struct {
    uint8_t note_type;
    uint16_t width, space;
    uint8_t level, OutMode, IctStartMode, res[3];
} __attribute__((aligned(1), packed)) ZCJ_PARAM_DEF, *PZCJ_PARAM_DEF;
typedef struct {
    uint8_t attribute;
    char ver_soft[64], ver_hard[64];
} __attribute__((aligned(1), packed)) ZCJ_PARAM_VER_DEF, *PZCJ_PARAM_VER_DEF;
typedef struct {
    uint8_t status, order[128];
} __attribute__((aligned(1), packed)) ZCJ_PAY_APPLY_REPLY, *PZCJ_PAY_APPLY_REPLY;
typedef struct {
    uint8_t channel;
    uint32_t number;
    char order[128];
} __attribute__((aligned(1), packed)) ZCJ_PAY_START_REPLY, *PZCJ_PAY_START_REPLY;
typedef struct {
    uint8_t channel, result;
    char order[128];
} __attribute__((aligned(1), packed)) ZCJ_PAY_START_RESULT_REPLY, *PZCJ_PAY_START_RESULT_REPLY;
typedef struct {
    uint8_t unique, channel;
    uint32_t inc, total;
} __attribute__((aligned(1), packed)) ZCJ_ACCOUNTS_UPDATE, *PZCJ_ACCOUNTS_UPDATE;
typedef struct {
    uint32_t dev_type, brt;
    uint8_t led_status;
    uint16_t pulse_width, pulse_space;
    uint8_t pluse_level, pluse_output, pluse_outputPort, array[5];
} __attribute__((aligned(1), packed)) ZCJ_DEV_TYPE_INFOR, *PZCJ_DEV_TYPE_INFOR;
typedef struct {
    uint8_t ch_sum;
    uint32_t ch_acc[12];
} __attribute__((aligned(1), packed)) ZCJ_NOTE_ACCS_TOTAL, *PZCJ_NOTE_ACCS_TOTAL;
typedef struct {
    char fileName[20];
    uint32_t fileTotal, fileOffset, upDataLen;
    char upData;
} __attribute__((aligned(1), packed)) ZJC_OPEN_UPDATE_DATA_T;
typedef struct {
    uint8_t subCmd;
    char fileName[20];
    uint32_t fileTotal, fileOffset, upDataLen;
    char result;
} __attribute__((aligned(1), packed)) ZJC_OPEN_UPDATE_RES_T;

void zcj_protocol_init(void);
void lyy_zcj_packet_parse(uint8_t *packet, uint16_t len);
int zcj_cmd_set_param(ZCJ_PARAM_DEF *param);
ZCJ_PARAM_DEF *zcj_param_get(void);
void zcj_note_type_to_string(char *note_string, uint8_t type);
uint8_t zcj_note_type_to_value(char *note_string);
int zcj_cmd_pay_start(char *order, uint8_t channel, uint32_t number);
int zcj_cmd_pay_apply(char *order);
int zcj_cmd_pay_apply_pulse(char *order);
void zcj_cmd_pay_start_pulse(char *order, uint8_t channel, uint32_t number);
int zcj_cmd_get_param(void);
void zcj_packet_screen_dtu_send(char *pack, uint16_t len);
int zcj_cmd_get_param_ver(void);
ZCJ_PARAM_VER_DEF *zcj_param_ver_get(void);
ZCJ_NOTE_ACCS_TOTAL *zcj_accounts_get(void);
int zcj_packet_cmd_total_acc_get(void);
void zcj_request_total_acc(void);
uint16_t api_pluse_getLevel(void);
void zcj_packet_dev_upgrade(char *pcFileName, uint32_t fileTotal, uint32_t ulOffset,
                            char *pcUpData, uint32_t ulUpDataLen);
#endif
