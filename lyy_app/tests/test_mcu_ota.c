/**
 * @file test_mcu_ota.c
 * @brief STM32 升级发送任务回归：块偏移、ACK 字段匹配、短 ACK、超时及资源/读取失败。
 * @note 模拟 RTOS 和对端应答，仅证明本机状态机路径，不确认对端实际烧录成功。
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "lyy_device.h"
#include "lyy_zcj.h"
#include "lyy_mcu_ota.h"
#include "HAL_Ota.h"
BOARD_OTA_PARAM_DEF board_ota_param_t;
static TaskFunction_t worker;
static unsigned packets, mismatch_checked;
static int token, fail_task, fail_read, omit_ack;
static uint32_t next_offset;
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return &token; }
int xSemaphoreTake(SemaphoreHandle_t h,uint32_t timeout)
{ (void)timeout; assert(h==&token); if(!token) return 0; token=0; return 1; }
void xSemaphoreGive(SemaphoreHandle_t h) { assert(h==&token); token=1; }
int xTaskCreate(TaskFunction_t cb,const char *name,uint32_t stack,void *arg,unsigned pri,void *handle)
{ (void)name;(void)stack;(void)arg;(void)pri;(void)handle; worker=cb;return !fail_task; }
void vTaskDelete(void *arg) { (void)arg; }
int HAL_lFlashAddrRead(uint32_t addr,uint8_t *data,size_t len)
{
    assert(addr==FILE_NAME_HW_DEVICE_OTA_LOAD+next_offset);
    if(fail_read) return -1;
    memset(data,0x5A,len); return 0;
}
void zcj_packet_dev_upgrade(char *name,uint32_t total,uint32_t offset,char *data,uint32_t len)
{
    assert(total==2500 && offset==next_offset && len<=1024 && data[0]==0x5A);
    ++packets;
    uint8_t frame[sizeof(ZJC_OPEN_UPDATE_RES_T)+6]={0};
    ZJC_OPEN_UPDATE_RES_T ack={0};
    ack.subCmd=ZCJ_SUB_CMD_DEV_UPGRADE;
    memcpy(ack.fileName,name,strlen(name));
    ack.fileTotal=total;ack.fileOffset=offset;ack.upDataLen=len;
    memcpy(frame+4,&ack,sizeof(ack));
    lyy_mcu_ota_ack(frame,sizeof(frame)-1); assert(!token);
    ack.fileOffset=offset+1;memcpy(frame+4,&ack,sizeof(ack));
    lyy_mcu_ota_ack(frame,sizeof(frame));assert(!token);++mismatch_checked;
    ack.fileOffset=offset;ack.result=1;memcpy(frame+4,&ack,sizeof(ack));
    lyy_mcu_ota_ack(frame,sizeof(frame));assert(!token);
    ack.result=0;memcpy(frame+4,&ack,sizeof(ack));
    if(!omit_ack) lyy_mcu_ota_ack(frame,sizeof(frame));
    next_offset+=len;
}
static void prepare(void)
{
    board_ota_param_t.file_size=2500;strcpy(board_ota_param_t.version,"SG0B1_STD_V1.0.6");
    packets=0;next_offset=0;token=0;fail_read=0;omit_ack=0;fail_task=0;
}
int main(void)
{
    assert(!lyy_mcu_ota_start());prepare();
    assert(lyy_mcu_ota_start() && lyy_mcu_ota_busy());
    assert(!lyy_mcu_ota_start());worker(NULL);
    assert(!lyy_mcu_ota_busy() && packets==3 && next_offset==2500 && mismatch_checked==3);
    prepare();omit_ack=1;assert(lyy_mcu_ota_start());worker(NULL);
    assert(packets==1 && !lyy_mcu_ota_busy());
    prepare();fail_read=1;assert(lyy_mcu_ota_start());worker(NULL);
    assert(packets==0 && !lyy_mcu_ota_busy());
    prepare();fail_task=1;assert(!lyy_mcu_ota_start() && !lyy_mcu_ota_busy());
    puts("STM32 OTA tests passed: block offsets, matching ACK, short/error ACK rejection, timeout, read/task failure, busy guard");
    return 0;
}
