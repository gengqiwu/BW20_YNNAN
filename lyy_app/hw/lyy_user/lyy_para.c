/**
 * @file lyy_para.c
 * @brief 设备及支付参数缓存、设备类型映射和默认服务器配置初始化。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "HAL_Log.h"
#include "HAL_System.h"
#include "APP_Param.h"
#include "lyy_para.h"

PARA_DEV_DEF paraDev;
PAY_PARAM_DEF pay_param_t;
const PARAGATE_DEF infor_server[SERVER_ADDR_INFOR_MAX] = {
    {NET_TYPE_LMQ, 8883, "mqtt.cadmem.com"},
    {NET_TYPE_LMQ, 1883, "mqtt.cadmem.com"},
    {NET_TYPE_LMQ, 8883, "sgiot.starthing.com"}
};
/**
 * @brief 在类型名称表中按配置字符串子串查找设备类型。
 * @return 首个匹配的类型下标；未匹配返回 DEVICE_TYPE_DEFAULT。
 * @note 匹配方向为表项名称包含 APP_pcGetDevType()，并非完整字符串相等。
 */
int get_dev_type_code(void)
{
    const DEV_TYPE_DEF types[] = {
        {"dev_type_default"}, {"dev_type_pulse"}, {"dev_type_level"},
        {"dev_type_uart_custom"}, {"dev_type_uart_wwj_lyy"},
        {"dev_type_uart_dbj_lyy"}, {"dev_type_uart_ndj_lyy"},
        {"dev_type_uart_amy_lyy"}, {"dev_type_uart_xyj_lyy"},
        {"dev_type_uart_note_lyy"}
    };
    for (uint8_t i = 0; i < sizeof(types) / sizeof(types[0]); i++)
        if (strstr(types[i].name, APP_pcGetDevType()) != NULL) return i;
    return DEVICE_TYPE_DEFAULT;
}
char *get_money_unit(void) { return pay_param_t.pkgs.currency_name; }
PAY_PARAM_DEF *get_pay_param(void) { return &pay_param_t; }
PAY_PACKAGES_DEF *get_pay_packages(void) { return &pay_param_t.pkgs; }
/**
 * @brief 保留扭蛋机套餐访问接口，本固件无对应缓存。
 * @return 始终返回 NULL，调用方不得解引用。
 */
PAY_PACKAGES_NDJ_DEF *get_pay_ndj_packages(void) { return NULL; }
/**
 * @brief 保留扭蛋机货道使能查询接口。
 * @param[in] x 未使用。
 * @return 固定值 1，不反映实际货道状态。
 */
uint8_t ndj_get_lane_enable(uint8_t x) { (void)x; return 1; }
/**
 * @brief 保留扭蛋机网络货道表访问接口。
 * @return 始终返回 NULL，本固件不提供该表。
 */
PLANE_INFOR_TABLE_DEF ndj_net_lane_infor(void) { return NULL; }
PARA_DEV_DEF *para_getDevInfor(void) { return &paraDev; }
PARAINTER_DEF *para_getInterInfor(void) { return &paraDev.inter; }
/**
 * @brief 从设备参数文件读取整个设备参数结构。
 * @param[out] arg 有效的设备参数缓冲区，不得为 NULL。
 * @return 原样传递 lyy_para_read 的返回值。
 */
int32_t lyy_dev_read(PPARA_DEV_DEF arg)
{ return lyy_para_read(FILE_NAME_PARA_DEVICE, &arg->init_mark, sizeof(*arg)); }
/**
 * @brief 将整个设备参数结构写入设备参数文件。
 * @param[in] arg 有效的设备参数结构，不得为 NULL。
 * @return 原样传递 lyy_para_write 的返回值。
 */
int32_t lyy_dev_write(PPARA_DEV_DEF arg)
{ return lyy_para_write(FILE_NAME_PARA_DEVICE, &arg->init_mark, sizeof(*arg)); }
/**
 * @brief 从预置服务器表复制地址、端口和网络类型。
 * @param[out] gate 目标服务器配置；有效 type 时须为非空指针。
 * @param[in] type 服务器表下标，越界时不修改目标。
 */
void set_serAddr(PPARAGATE_DEF gate, uint8_t type)
{
    if (type >= SERVER_ADDR_INFOR_MAX) return;
    strcpy(gate->url, infor_server[type].url);
    gate->port = infor_server[type].port;
    gate->type = infor_server[type].type;
}
/**
 * @brief 读取设备配置，初始化缺少标记的参数并迁移旧服务器地址。
 * @note init_mark 非 D 时清零并写默认值：DEVICE_TYPE_DEFAULT/dev_type_default、登录标志 4、正式服务器及串口 38400。
 * @note 默认类型不强制设为纸钞类型；旧 ehw.leyaoyao.com:4900 配置迁移到正式 MQTT 服务器并写回。
 * @warning 读取失败仅记录日志；文件写入返回值未检查。
 */
void para_deviceInit(void)
{
    int32_t ret = lyy_dev_read(&paraDev);
    if (ret < 0) LYY_LOGD("lyy_dev_read devpara is fail:%d", ret);
    if (paraDev.init_mark != 'D')
    {
        memset(&paraDev, 0, sizeof(paraDev));
        paraDev.init_mark = 'D';
        strcpy(paraDev.id, " no device id,need config ");
        paraDev.type = DEVICE_TYPE_DEFAULT;
        strcpy(paraDev.dev_type, "dev_type_default");
        paraDev.gate.type = NET_TYPE_DEFAULT;
        paraDev.login_flag = 4;
        set_serAddr(&paraDev.gate, SERVER_ADDR_INFOR_MQTT_MOT_SG_RELEASE);
        paraDev.inter.brt = 38400;
        paraDev.inter.width = 50;
        paraDev.inter.gap = 200;
        paraDev.flow.mode = 3;
        paraDev.flow.cycle = 2;
        paraDev.flow.number = 400;
        lyy_dev_write(&paraDev);
    }
    if (paraDev.gate.port == 4900 && strstr(paraDev.gate.url, "ehw.leyaoyao.com") != NULL)
    {
        set_serAddr(&paraDev.gate, SERVER_ADDR_INFOR_MQTT_MOT_SG_RELEASE);
        lyy_dev_write(&paraDev);
    }
}
