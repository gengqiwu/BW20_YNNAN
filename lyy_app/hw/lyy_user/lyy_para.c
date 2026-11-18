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
PAY_PACKAGES_NDJ_DEF *get_pay_ndj_packages(void) { return NULL; }
uint8_t ndj_get_lane_enable(uint8_t x) { (void)x; return 1; }
PLANE_INFOR_TABLE_DEF ndj_net_lane_infor(void) { return NULL; }
PARA_DEV_DEF *para_getDevInfor(void) { return &paraDev; }
PARAINTER_DEF *para_getInterInfor(void) { return &paraDev.inter; }
int32_t lyy_dev_read(PPARA_DEV_DEF arg)
{ return lyy_para_read(FILE_NAME_PARA_DEVICE, &arg->init_mark, sizeof(*arg)); }
int32_t lyy_dev_write(PPARA_DEV_DEF arg)
{ return lyy_para_write(FILE_NAME_PARA_DEVICE, &arg->init_mark, sizeof(*arg)); }
void set_serAddr(PPARAGATE_DEF gate, uint8_t type)
{
    if (type >= SERVER_ADDR_INFOR_MAX) return;
    strcpy(gate->url, infor_server[type].url);
    gate->port = infor_server[type].port;
    gate->type = infor_server[type].type;
}
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
