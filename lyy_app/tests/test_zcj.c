/**
 * @file test_zcj.c
 * @brief 真实 D6 编解码主机回归：参数 ACK、订单/通道关联、32 位接单结果、现金 ACK/去重及配置字段。
 * @note 用模拟传输同步注入应答，另覆盖非阻塞总账查询与短帧，不替代真实并发和硬件联调。
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lyy_zcj.h"
#include "lyy_device.h"
#include "lyy_para.h"
#include "APP_Param.h"
#include "APP_Led.h"
#include "HAL_Uart.h"
#include "HAL_Sem.h"
LyyHwMark_t g_xlyyHwMark;
union U ulPulsetype;
static APP_PULSE_T pulse;
static int token, mutex, suppress;
static unsigned signals, cash_events, acks, final_events;
static uint8_t last_frame[1200];
static uint32_t last_len;
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &mutex; }
int xSemaphoreTake(SemaphoreHandle_t h,uint32_t ms) { (void)ms; assert(h==&mutex); return 1; }
void xSemaphoreGive(SemaphoreHandle_t h) { assert(h==&mutex); }
uint32_t lyy_semNewV2(SEM_TYPE *sem,int init) { (void)init;*sem=&token;token=0;return 0; }
uint8_t lyy_semWaitTryV2(SEM_TYPE sem,uint32_t ms) { (void)ms;assert(sem==&token);return token; }
void lyy_semSignalV2(SEM_TYPE sem) { if(sem) {token=1;++signals;} }
void vTaskDelay(uint32_t ms) { (void)ms; }
uint8_t check_xor_memory(const uint8_t *p,uint16_t n) { uint8_t v=0;while(n--)v^=*p++;return v; }
int get_dev_type_code(void) { return DEVICE_TYPE_UART_NOTE; }
uint32_t APP_ulGetParamBaudRate(void) { return 38400; }
APP_PULSE_T *APP_pxlGetParamePulse(void) { return &pulse; }
uint8_t get_led_status(void) { return LED_STATUS_LOGIN_SUCCESS; }
void tjc_packet_parse(uint8_t *data,uint16_t len) { (void)data;(void)len; }
void thing_ota_inform_version(char *version,char *module) { (void)version;(void)module; }
void property_post_dev_mcu_version(char *soft,char *hard) { (void)soft;(void)hard; }
void lyy_mcu_ota_ack(const uint8_t *frame,uint16_t len) { (void)frame;(void)len; }
void event_post_account_note(uint32_t inc,uint32_t sum,char *pay,char *order,char *event,int32_t channel)
{ (void)pay;(void)order;(void)event;assert(inc==5 && sum==10 && channel==1);++cash_events; }
void event_post_account_pulse(uint32_t inc,uint32_t sum,char *pay,char *order,char *event)
{ (void)inc;(void)sum;(void)pay;(void)order;(void)event;assert(0); }
void event_post_start_result_note(uint32_t result,uint32_t channel,char *order)
{ assert(result==1 && channel==2 && !strcmp(order,"ORDER"));++final_events; }
static void respond(uint8_t cmd,const void *payload,size_t n)
{
    uint8_t frame[200]={0xAA,0,1,0xD6,0};
    frame[1]=(uint8_t)(n+4);frame[4]=cmd;
    if(n)memcpy(frame+5,payload,n);
    lyy_zcj_packet_parse(frame,(uint16_t)n+7);
}
void HAL_vUartSend(uint8_t port,uint8_t *p,uint32_t n,uint32_t timeout)
{
    (void)timeout;assert(port==HAL_UART_MAIN_BOARD && n<=sizeof(last_frame));
    assert(p[0]==0xAA && p[n-1]==0xDD && p[n-2]==check_xor_memory(p+1,n-3));
    memcpy(last_frame,p,n);last_len=n;
    if(p[4]==ZCJ_SUB_CMD_DEV_ACCOUNTS_UPDATE) {++acks;return;}
    if(suppress)return;
    if(p[4]==ZCJ_SUB_CMD_PARAM_SET) respond(p[4],NULL,0);
    if(p[4]==ZCJ_SUB_CMD_PARAM_GET) {
        ZCJ_PARAM_DEF param={0};param.level=1;param.width=50;param.space=200;
        respond(p[4],&param,sizeof(param));
    }
    if(p[4]==ZCJ_SUB_CMD_PAY_APPLY) {
        ZCJ_PAY_APPLY_REPLY reply={0};reply.status=1;memcpy(reply.order,p+5,128);
        reply.order[0]^=1;respond(p[4],&reply,sizeof(reply));assert(token==0);
        reply.order[0]^=1;respond(p[4],&reply,sizeof(reply));
    }
    if(p[4]==ZCJ_SUB_CMD_PAY_START) {
        uint8_t payload[133]={0};payload[0]=p[5];payload[1]=1;memcpy(payload+5,p+10,128);
        payload[5]^=1;respond(p[4],payload,sizeof(payload));assert(token==0);
        payload[5]^=1;payload[0]^=1;respond(p[4],payload,sizeof(payload));assert(token==0);
        payload[0]^=1;respond(p[4],payload,sizeof(payload));
    }
    if(p[4]==ZCJ_SUB_CMD_DEV_TOTAL_ACCOUNTS) {
        uint8_t payload[41]={10};respond(p[4],payload,sizeof(payload));
    }
}
int main(void)
{
    zcj_protocol_init();
    ZCJ_PARAM_DEF param={0};assert(zcj_cmd_set_param(&param)==1);
    assert(zcj_cmd_get_param()==1 && api_pluse_getLevel()==1);
    assert(zcj_cmd_pay_apply("ORDER")==1 && g_xlyyHwMark.applyResult==1);
    assert(zcj_cmd_pay_start("ORDER",2,3)==1 && g_xlyyHwMark.startResult==1);
    suppress=1;assert(zcj_cmd_pay_apply("TIMEOUT")==0);suppress=0;
    assert(zcj_packet_cmd_total_acc_get()==1 && zcj_accounts_get()->ch_sum==10);
    zcj_accounts_get()->ch_sum=0;
    zcj_request_total_acc();
    assert(zcj_accounts_get()->ch_sum==10);
    suppress=1;
    ZCJ_ACCOUNTS_UPDATE cash={3,1,5,10};
    respond(ZCJ_SUB_CMD_DEV_ACCOUNTS_UPDATE,&cash,sizeof(cash));
    respond(ZCJ_SUB_CMD_DEV_ACCOUNTS_UPDATE,&cash,sizeof(cash));
    assert(acks==2 && cash_events==1);
    cash.unique=4;respond(ZCJ_SUB_CMD_DEV_ACCOUNTS_UPDATE,&cash,sizeof(cash));assert(cash_events==2);
    ZCJ_PAY_START_RESULT_REPLY final={2,1,"ORDER"};
    respond(ZCJ_SUB_CMD_PAY_RESULT_UPDATE,&final,sizeof(final));assert(final_events==1);
    pulse.ulPulseWidth=50;pulse.ulPulseSpace=200;ulPulsetype.bytes[0]=6;ulPulsetype.bytes[1]=2;
    respond(ZCJ_SUB_CMD_DEV_TYPE_GET,NULL,0);
    assert(last_len==sizeof(ZCJ_DEV_TYPE_INFOR)+7);
    ZCJ_DEV_TYPE_INFOR info;memcpy(&info,last_frame+5,sizeof(info));
    assert(info.dev_type==9 && info.brt==38400 && info.pluse_output==1 && info.pluse_outputPort==2 && info.pluse_level==1);
    unsigned before=signals;uint8_t short_packet[7]={0xAA,4,1,0xD6,3,0,0};
    lyy_zcj_packet_parse(short_packet,sizeof(short_packet));assert(signals==before);
    puts("D6 tests passed: parameter ACK, matching orders/channels, u32 start result, timeout, cash ACK/dedup, final event, config fields, short frames");
    return 0;
}
