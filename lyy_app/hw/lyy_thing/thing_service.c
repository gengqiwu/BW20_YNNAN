/* ESP32C2_ZCJ_V1.0.4 service model. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "HAL_Log.h"
#include "HAL_System.h"
#include "APP_Param.h"
#include "cJSON.h"
#include "thing_iot.h"
#include "thing_service.h"
#include "thing_property.h"
#include "lyy_tjc.h"
#include "lyy_para.h"
#include "lyy_start.h"

int thing_service_payment_code(cJSON *params, char *id);
int thing_service_qrcode_dynamic(cJSON *params, char *id);
int thing_service_pay_result(cJSON *params, char *id);
int thing_service_lane_infor_get(cJSON *params, char *id);
int thing_service_packages_set_ndj(cJSON *params, char *id);
int thing_service_lane_infor_set(cJSON *params, char *id);
int thing_service_restart(cJSON *params, char *id);
int thing_service_pay_apply(cJSON *params, char *id);
int thing_service_pay_start(cJSON *params, char *id);
int thing_service_pay_apply_wwj(cJSON *params, char *id);
int thing_service_pay_start_dbj_note(cJSON *params, char *id);
const SERVICE_METHOD_DEF moethod_service[] = {
    {"pay_apply", thing_service_pay_apply},
    {"pay_start", thing_service_pay_start},
    {"pay_qr_code", thing_service_payment_code},
    {"qrcode_post", thing_service_qrcode_dynamic},
    {"pay_notice", thing_service_pay_result},
    {"device_position_info", thing_service_lane_infor_get},
    {"update_position_pay_package", thing_service_packages_set_ndj},
    {"pay_package", thing_service_packages_set_ndj},
    {"update_position_info", thing_service_lane_infor_set},
    {"reboot", thing_service_restart},
    {"event_notification", NULL}
};
char dbj_start_type[64];
char *get_start_type(void) { return dbj_start_type; }
void set_start_type(char *type)
{
    memset(dbj_start_type, 0, sizeof(dbj_start_type));
    if (type != NULL) strncpy(dbj_start_type, type, sizeof(dbj_start_type) - 1);
}
char *get_apply_start_result_type(uint16_t code)
{
    static char result[30];
    const char *names[] = {"fail", "success", "fault", "offline", "starting", "timeout", "start_normally", "startup_failed"};
    strcpy(result, code < 8 ? names[code] : "other");
    return result;
}
int thing_service_parse(cJSON *params, char *method, char *id)
{
    if (method == NULL) return -1;
    for (uint8_t i = 0; i < sizeof(moethod_service) / sizeof(moethod_service[0]); i++)
        if (strstr(method, moethod_service[i].method) != NULL)
            return moethod_service[i].parse ? moethod_service[i].parse(params, id) : -1;
    return -1;
}
int thing_service_pay_apply(cJSON *params, char *id)
{ thing_service_pay_apply_wwj(params, id); return 1; }
int thing_service_pay_start(cJSON *params, char *id)
{
    switch (get_dev_type_code()) {
    case DEVICE_TYPE_LEVEL:
    case DEVICE_TYPE_PULSE: thing_service_pay_start_pulse(params, id); break;
    case DEVICE_TYPE_UART_NOTE: thing_service_pay_start_dbj_note(params, id); break;
    default: return -1;
    }
    return 1;
}
int thing_service_packages_set_std(cJSON *params, char *id)
{
    /* Unused in the dedicated ZCJ model (matches reference behavior). */
    (void)params; (void)id;
    return -1;
}
char thing_service_qrcode_dynamic_ndj(cJSON *params, char *id)
{ (void)params; (void)id; return 1; }
int thing_service_qrcode_dynamic_std(cJSON *params, char *id)
{
    cJSON *order = cJSON_GetObjectItem(params, "order");
    cJSON *url = cJSON_GetObjectItem(params, "url");
    cJSON *money = cJSON_GetObjectItem(params, "money");
    cJSON *unique = cJSON_GetObjectItem(params, "unique_code");
    if (order == NULL || order->type != cJSON_String || url == NULL || url->type != cJSON_String ||
        money == NULL || money->type != cJSON_String) return -1;
    if (unique == NULL || unique->type != cJSON_String) return -2;
    if (cmp_pay_url_uinque_code(0, unique->valuestring)) return -1;
    lyy_tjc_pay_url_show(url->valuestring, money->valuestring);
    thing_service_invoke_reply(NULL, "success", id, "qrcode_post");
    return 1;
}
int thing_service_qrcode_dynamic(cJSON *params, char *id)
{
    if (get_dev_type_code() == DEVICE_TYPE_UART_NDJ) thing_service_qrcode_dynamic_ndj(params, id);
    else thing_service_qrcode_dynamic_std(params, id);
    return 1;
}
int thing_service_lane_infor_get(cJSON *params, char *id)
{ (void)params; (void)id; return 1; }
int thing_service_lane_infor_set(cJSON *params, char *id)
{ (void)params; (void)id; return 1; }
int thing_service_packages_set_ndj(cJSON *params, char *id)
{ (void)params; (void)id; return 1; }
void thing_service_reply_ndj_lane_get(uint8_t lane, uint8_t status, uint8_t coins, uint8_t time, char *id)
{
    cJSON *ack = cJSON_CreateObject();
    if (ack == NULL) return;
    cJSON_AddNumberToObject(ack, "position_x", lane);
    cJSON_AddNumberToObject(ack, "position_y", 0);
    cJSON_AddNumberToObject(ack, "status", status);
    cJSON_AddNumberToObject(ack, "play_time", time);
    cJSON_AddNumberToObject(ack, "startup_unit", coins);
    thing_service_invoke_reply(ack, "success", id, "device_position_info");
}
int thing_service_pay_apply_wwj(cJSON *params, char *id)
{
    cJSON *order = cJSON_GetObjectItem(params, "order");
    if (order == NULL || order->type != cJSON_String) return -1;
    return api_std_start_entryQue(START_TYPE_PAY_APPLY, 0, order->valuestring, 0, 0, id);
}
void thing_service_reply_pay_apply(char *order, uint16_t result, char *id)
{
    cJSON *data = cJSON_CreateObject();
    if (data == NULL) return;
    cJSON_AddStringToObject(data, "order", order);
    cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
    thing_service_invoke_reply(data, "success", id, "pay_apply");
}
void thing_service_reply_pay_apply_note(char *order, uint16_t result, char *id)
{ thing_service_reply_pay_apply(order, result, id); }
int thing_service_pay_start_pulse(cJSON *params, char *id)
{
    uint32_t coins = 0;
    cJSON *number = cJSON_GetObjectItem(params, "number");
    cJSON *order = cJSON_GetObjectItem(params, "order");
    if (!number || number->type != cJSON_Number || number->valuedouble < 1 ||
        number->valuedouble > 65535 || number->valuedouble != number->valueint) return -1;
    coins = number->valueint;
    if (order == NULL || order->type != cJSON_String || !order->valuestring[0] || strlen(order->valuestring) >= 64) return -1;
    return api_std_start_entryQue(START_TYPE_PAY_START, coins, order->valuestring, 0, 0, id);
}
int thing_service_pay_start_dbj_note(cJSON *params, char *id)
{
    cJSON *channel = cJSON_GetObjectItem(params, "channel");
    cJSON *number = cJSON_GetObjectItem(params, "number");
    cJSON *order = cJSON_GetObjectItem(params, "order");
    cJSON *type = cJSON_GetObjectItem(params, "type");
    if (channel == NULL || channel->type != cJSON_Number) return -2;
    if (number == NULL || number->type != cJSON_Number || number->valuedouble < 1 ||
        number->valuedouble > 65535 || number->valuedouble != number->valueint ||
        channel->valuedouble < 0 || channel->valuedouble > 255 || channel->valuedouble != channel->valueint ||
        order == NULL || order->type != cJSON_String || !order->valuestring[0] || strlen(order->valuestring) >= 64) return -1;
    if (type != NULL && type->type == cJSON_String) set_start_type(type->valuestring);
    return api_std_start_entryQue(START_TYPE_PAY_START, number->valueint, order->valuestring, channel->valueint, 0, id);
}
void thing_service_reply_pay_start_note(char *order, uint16_t result, char *id, uint8_t channel)
{
    cJSON *data = cJSON_CreateObject();
    if (data == NULL) return;
    cJSON_AddStringToObject(data, "order", order);
    cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
    cJSON_AddNumberToObject(data, "channel", channel);
    cJSON_AddStringToObject(data, "type", get_start_type());
    thing_service_invoke_reply(data, "success", id, "pay_start");
}
int thing_service_payment_code(cJSON *params, char *id)
{
    (void)params;
    thing_service_invoke_reply(NULL, "success", id, "pay_qr_code");
    return 0;
}
int thing_service_pay_result(cJSON *params, char *id)
{
    cJSON *status = cJSON_GetObjectItem(params, "status");
    cJSON *data;
    if (status == NULL || status->type != cJSON_String) return -1;
    lyy_tjc_page_pay_reuslt();
    data = cJSON_CreateObject();
    if (data != NULL) {
        cJSON_AddStringToObject(data, "result", "success");
        thing_service_invoke_reply(data, "success", id, "pay_notice");
    }
    return 1;
}
int thing_service_restart(cJSON *params, char *id)
{
    cJSON *obj, *data;
    if (params == NULL) return -1;
    obj = cJSON_GetObjectItem(params, "obj");
    if (obj != NULL && obj->type == cJSON_String) {
        data = cJSON_CreateObject();
        if (data != NULL) {
            cJSON_AddStringToObject(data, "result", "success");
            thing_service_invoke_reply(data, "success", id, "reboot");
        }
        if (strstr(obj->valuestring, "box")) HAL_vReset("system restart", 1000);
    }
    return 1;
}
