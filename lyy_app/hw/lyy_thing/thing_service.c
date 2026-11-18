/**
 * @file thing_service.c
 * @brief 纸钞盒物模型服务分发、支付请求校验及即时应答封装。
 * @note pay_apply/pay_start 应答描述申请或即时接单状态；最终执行结果由主板 0x07 触发 start_result 事件。
 */
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
/**
 * @brief 获取最近保存的启动类型。
 * @return 模块共享缓冲区，不得释放，后续设置会覆盖其内容。
 */
char *get_start_type(void) { return dbj_start_type; }
/**
 * @brief 保存启动类型，最多复制 63 字节。
 * @param[in] type 启动类型字符串；NULL 表示清空。
 * @note 保存的是模块共享状态，并非逐订单副本。
 */
void set_start_type(char *type)
{
    memset(dbj_start_type, 0, sizeof(dbj_start_type));
    if (type != NULL) strncpy(dbj_start_type, type, sizeof(dbj_start_type) - 1);
}
/**
 * @brief 将申请或启动结果码转换为云端字符串。
 * @param[in] code 0～7 对应既定结果，其他值映射为 other。
 * @return 静态缓冲区；后续调用会覆盖，不得释放。
 */
char *get_apply_start_result_type(uint16_t code)
{
    static char result[30];
    const char *names[] = {"fail", "success", "fault", "offline", "starting", "timeout", "start_normally", "startup_failed"};
    strcpy(result, code < 8 ? names[code] : "other");
    return result;
}
/**
 * @brief 按服务表顺序匹配方法名子串并调用处理入口。
 * @param[in] params 服务参数，由具体处理入口解释。
 * @param[in] method 方法字符串，可为 NULL。
 * @param[in] id 服务请求标识，供应答关联。
 * @return 处理入口原返回值；方法为空、未匹配或入口为空时为 -1。
 */
int thing_service_parse(cJSON *params, char *method, char *id)
{
    if (method == NULL) return -1;
    for (uint8_t i = 0; i < sizeof(moethod_service) / sizeof(moethod_service[0]); i++)
        if (strstr(method, moethod_service[i].method) != NULL)
            return moethod_service[i].parse ? moethod_service[i].parse(params, id) : -1;
    return -1;
}
/**
 * @brief 将支付申请交给通用入队入口。
 * @param[in] params 含字符串 order 的服务参数。
 * @param[in] id 请求标识。
 * @retval 1 固定返回值，不传递下层校验或入队结果。
 * @note 申请应答由启动事件回调发送，不表示已经执行出钞。
 */
int thing_service_pay_apply(cJSON *params, char *id)
{ thing_service_pay_apply_wwj(params, id); return 1; }
/**
 * @brief 按设备类型分发脉冲、电平或纸钞启动请求。
 * @param[in] params 启动参数，具体字段由分支入口校验。
 * @param[in] id 请求标识。
 * @retval -1 设备类型不支持。
 * @retval 1 已调用支持的分支；不传递分支返回值，也不表示启动成功。
 */
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
/**
 * @brief 保留通用套餐服务接口，本固件不执行配置。
 * @param[in] params 未使用。
 * @param[in] id 未使用。
 * @retval -1 固定返回不支持。
 */
int thing_service_packages_set_std(cJSON *params, char *id)
{
    /* Unused in the dedicated ZCJ model (matches reference behavior). */
    (void)params; (void)id;
    return -1;
}
/**
 * @brief 保留扭蛋机动态二维码兼容入口，不执行处理。
 * @param[in] params 未使用。
 * @param[in] id 未使用。
 * @retval 1 固定返回，不发送应答或显示二维码。
 */
char thing_service_qrcode_dynamic_ndj(cJSON *params, char *id)
{ (void)params; (void)id; return 1; }
/**
 * @brief 校验通用动态二维码并请求屏幕显示。
 * @param[in] params 含字符串 order、url、money 和 unique_code 的参数。
 * @param[in] id qrcode_post 应答关联标识。
 * @retval -1 订单、链接或金额字段无效，或支付唯一码不匹配。
 * @retval -2 支付唯一码缺失或非字符串。
 * @retval 1 已请求显示并发送成功应答。
 */
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
/**
 * @brief 分发动态二维码到扭蛋机兼容或通用入口。
 * @param[in] params 动态二维码参数。
 * @param[in] id 请求标识。
 * @retval 1 固定返回，不传递下层校验结果。
 */
int thing_service_qrcode_dynamic(cJSON *params, char *id)
{
    if (get_dev_type_code() == DEVICE_TYPE_UART_NDJ) thing_service_qrcode_dynamic_ndj(params, id);
    else thing_service_qrcode_dynamic_std(params, id);
    return 1;
}
/**
 * @brief 保留货道查询服务入口，本固件不查询也不应答。
 * @param[in] params 未使用。
 * @param[in] id 未使用。
 * @retval 1 固定返回。
 */
int thing_service_lane_infor_get(cJSON *params, char *id)
{ (void)params; (void)id; return 1; }
/**
 * @brief 保留货道设置服务入口，本固件不修改配置也不应答。
 * @param[in] params 未使用。
 * @param[in] id 未使用。
 * @retval 1 固定返回。
 */
int thing_service_lane_infor_set(cJSON *params, char *id)
{ (void)params; (void)id; return 1; }
/**
 * @brief 保留扭蛋机套餐服务入口，本固件不修改套餐也不应答。
 * @param[in] params 未使用。
 * @param[in] id 未使用。
 * @retval 1 固定返回。
 */
int thing_service_packages_set_ndj(cJSON *params, char *id)
{ (void)params; (void)id; return 1; }
/**
 * @brief 封装货道查询应答，纵坐标固定为 0。
 * @param[in] lane 货道横坐标。
 * @param[in] status 货道状态。
 * @param[in] coins 启动单位。
 * @param[in] time 游戏时间。
 * @param[in] id 请求标识。
 * @note JSON 分配失败时直接返回，不发送应答。
 */
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
/**
 * @brief 校验订单字段并尝试提交支付申请事件。
 * @param[in] params 含字符串 order 的参数。
 * @param[in] id 请求标识，入队函数会复制并截断到缓冲区容量。
 * @return order 缺失或类型错误返回 -1；否则返回入队入口结果 0 或 1。
 * @note 返回 1 仅表示入队调用完成，不保证事件送达或主板受理。
 */
int thing_service_pay_apply_wwj(cJSON *params, char *id)
{
    cJSON *order = cJSON_GetObjectItem(params, "order");
    if (order == NULL || order->type != cJSON_String) return -1;
    return api_std_start_entryQue(START_TYPE_PAY_APPLY, 0, order->valuestring, 0, 0, id);
}
/**
 * @brief 发送支付申请状态应答。
 * @param[in] order 订单号。
 * @param[in] result 申请结果码。
 * @param[in] id 原请求标识。
 * @note 外层状态固定 success，业务结果在 result 字段；分配失败不发送。
 */
void thing_service_reply_pay_apply(char *order, uint16_t result, char *id)
{
    cJSON *data = cJSON_CreateObject();
    if (data == NULL) return;
    cJSON_AddStringToObject(data, "order", order);
    cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
    thing_service_invoke_reply(data, "success", id, "pay_apply");
}
/**
 * @brief 通过通用封装发送纸钞支付申请应答。
 * @param[in] order 订单号。
 * @param[in] result 申请结果码。
 * @param[in] id 原请求标识。
 * @note 此应答不是最终出钞结果。
 */
void thing_service_reply_pay_apply_note(char *order, uint16_t result, char *id)
{ thing_service_reply_pay_apply(order, result, id); }
/**
 * @brief 校验脉冲启动订单并尝试提交启动事件。
 * @param[in] params number 须为 1～65535 整数，order 须非空且不足 64 字节。
 * @param[in] id 请求标识。
 * @return 校验失败为 -1；否则为入队入口返回值 0 或 1。
 * @note 此处不发送最终成功事件；硬件执行结果另行上报。
 */
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
/**
 * @brief 校验纸钞启动参数，保存可选类型并尝试提交事件。
 * @param[in] params channel 为 0～255 整数，number 为 1～65535 整数；order 非空且不足 64 字节；type 可选。
 * @param[in] id 请求标识。
 * @retval -2 channel 缺失或不是数值。
 * @retval -1 数量、通道范围或整数性、订单校验失败。
 * @return 校验通过后返回入队入口的 0 或 1，不代表主板接单成功。
 * @note type 未提供字符串时保留上次值；最终结果由主板 0x07 事件上报。
 */
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
/**
 * @brief 发送纸钞启动的即时接单应答。
 * @param[in] order 订单号。
 * @param[in] result 即时结果码，正常接单映射为 start_normally，拒绝为 startup_failed。
 * @param[in] id 原请求标识。
 * @param[in] channel 原请求通道。
 * @note type 读取模块共享缓存；JSON 分配失败不发送。此应答与 0x07 的 start_result 最终事件不同。
 */
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
/**
 * @brief 对静态支付二维码服务发送空成功应答。
 * @param[in] params 未使用，不更新二维码。
 * @param[in] id 请求标识。
 * @retval 0 固定返回。
 */
int thing_service_payment_code(cJSON *params, char *id)
{
    (void)params;
    thing_service_invoke_reply(NULL, "success", id, "pay_qr_code");
    return 0;
}
/**
 * @brief 收到支付通知后切换屏幕支付结果状态。
 * @param[in] params 须包含字符串 status；不依据其具体值区分支付结果。
 * @param[in] id pay_notice 应答关联标识。
 * @retval -1 status 缺失或非字符串。
 * @retval 1 已请求结果显示；即使应答 JSON 分配失败也返回此值。
 */
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
/**
 * @brief 处理重启服务，obj 含 box 子串时安排盒子复位。
 * @param[in] params 可包含字符串 obj。
 * @param[in] id reboot 应答关联标识。
 * @retval -1 params 为空。
 * @retval 1 其余情况；不保证存在 obj、已应答或已复位。
 */
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
