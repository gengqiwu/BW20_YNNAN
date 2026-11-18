#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "HAL_System.h"
#include "HAL_Uart.h"
#include "HAL_Flash.h"
#include "APP_Led.h"
#include "APP_Param.h"
#include "APP_Factory.h"
#include "lyy_start.h"

static uint32_t tick;
static unsigned resets, acknowledgements, prompts;
static int write_failure;
static uint8_t led;
static vRecvFunc recv_cb;
static struct HalSoftTimer *poll_timer, *prompt_timer;
static struct { char name[32]; uint8_t data[1024]; size_t len; } files[16];

SemaphoreHandle_t xSemaphoreCreateMutex(void) { return (void *)1; }
int xSemaphoreTake(SemaphoreHandle_t h, uint32_t timeout) { (void)timeout; return h != NULL; }
void xSemaphoreGive(SemaphoreHandle_t h) { assert(h); }
uint32_t HAL_ulGetSysTick(void) { return tick; }
uint8_t HAL_ucTickTimeout(uint32_t start, uint32_t timeout) { return (uint32_t)(tick-start) >= timeout; }
void HAL_vReset(char *why, uint32_t ms) { (void)why; (void)ms; ++resets; }
void HAL_vSoftTimeInit(struct HalSoftTimer *t, void (*cb)(void *), unsigned long repeat, void *arg)
{
    t->vTimeoutCb=cb; t->pxArg=arg; t->ullRepeat=repeat;
    if(repeat==20) poll_timer=t;
    if(repeat==4000) prompt_timer=t;
}
void set_led_status(uint8_t status) { led=status; }
uint8_t get_led_status(void) { return led; }
void APP_vLedSstSta(uint8_t type, uint8_t status) { (void)type; (void)status; }
void HAL_vUartRecvProc(uint8_t port, vRecvFunc cb) { assert(port==HAL_UART_MAIN_BOARD); recv_cb=cb; }
void HAL_vUartSend(uint8_t port, uint8_t *data, uint32_t len, uint32_t timeout)
{
    (void)port; (void)timeout;
    if(len>=15 && !memcmp(data,"\r\ndevice id is ok",15)) ++acknowledgements;
    if(len>=23 && !memcmp(data,"write device_id:include",23)) ++prompts;
}
static int slot(const char *name)
{
    for(int i=0;i<16;++i) if(!strcmp(files[i].name,name)) return i;
    for(int i=0;i<16;++i) if(!files[i].name[0]) { strcpy(files[i].name,name); return i; }
    assert(0); return 0;
}
int HAL_vFileWrite(const char *name,uint8_t *data,size_t len)
{
    if(write_failure) return -1;
    int i=slot(name); assert(len<=sizeof(files[i].data));
    memcpy(files[i].data,data,len); files[i].len=len; return (int)len;
}
int HAL_vFileRead(const char *name,uint8_t *data,size_t len)
{
    int i=slot(name); if(len>files[i].len) len=files[i].len;
    memcpy(data,files[i].data,len); return (int)len;
}
static void send_text(const char *text)
{
    tick+=300; recv_cb((uint8_t *)text,strlen(text));
    tick+=110; poll_timer->vTimeoutCb(NULL);
}
int main(void)
{
    assert(APP_lParamInit()==LYY_READ_PARAM_FAIL);
    APP_vTestInit(); assert(recv_cb && poll_timer && prompt_timer);
    prompt_timer->vTimeoutCb(NULL); assert(prompts==2 && led==LED_STATUS_NO_DEVID);
    send_text("not a command"); assert(resets==0);
    send_text("write device_id:abc"); assert(resets==0);
    send_text("write device_id:AB CD"); assert(resets==0);
    send_text("write device_id:ABCD"); assert(resets==1 && acknowledgements==1);
    assert(!strcmp(APP_pcGetParamDevId(),"ABCD"));
    assert(APP_ulGetParamBaudRate()==38400);
    assert(APP_pxlGetParamePulse()->ulPulseWidth==50);
    assert(APP_pxlGetParamePulse()->ulPulseSpace==200);
    assert(ulPulsetype.bytes[0]==START_TYPE_COIN && ulPulsetype.bytes[1]==PULSE_ZCJ_1);
    send_text("write device_id:01234567890123456789012345678901");
    assert(resets==2 && strlen(APP_pcGetParamDevId())==32);
    send_text("write device_id:012345678901234567890123456789012"); assert(resets==2);
    tick+=300; recv_cb((uint8_t *)"write device_",13);
    tick+=10; recv_cb((uint8_t *)"id:SPLIT",8);
    tick+=110; poll_timer->vTimeoutCb(NULL);
    assert(resets==3 && !strcmp(APP_pcGetParamDevId(),"SPLIT"));
    write_failure=1; send_text("write device_id:FAIL"); assert(resets==3 && acknowledgements==3);
    write_failure=0;
    uint8_t legacy[33]={ 'D' }; memcpy(legacy+1,"LEGACY",6);
    HAL_vFileWrite("APP_DEV_INFO",legacy,sizeof(legacy));
    assert(APP_lParamInit()==LYY_SUCCESS && !strcmp(APP_pcGetParamDevId(),"LEGACY"));
    assert(APP_lPulseParamSetInfo(0x00000206)==LYY_SUCCESS);
    ulPulsetype.value=0; assert(APP_lPulseParaInit()==LYY_SUCCESS && ulPulsetype.value==0x00000206);
    puts("factory tests passed: blank flash, ASCII prompts, ID rules/bounds, split input, write failure, legacy ID, pulse persistence");
    return 0;
}
