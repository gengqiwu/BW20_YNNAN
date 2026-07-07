/*
 * @author: e.he
 * @Date: 2024-07-10 16:29:48
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-10-09 16:31:10
 * @Description: 
 * 待机状态->启动成功:qhxyj_devSta.isAlarm:0不变
 *                     qhxyj_devSta.isRun:0->1
 *                 qhxyj_devSta.isAutoRun:0->1
 *                   qhxyj_devSta.isReady:0->1 
 * 
 * 待机状态->启动失败: qhxyj_devSta.isAlarm:0不变
 *                     qhxyj_devSta.isRun:0不变
 *                 qhxyj_devSta.isAutoRun:0不变
 *                   qhxyj_devSta.isReady:0不变 
 * 
 * 正常运行->发生故障，机器停止运行:  qhxyj_devSta.isAlarm:0->1
 *                                    qhxyj_devSta.isRun:1->0
 *                                qhxyj_devSta.isAutoRun:1->0
 *                                  qhxyj_devSta.isReady:1->0
 * 
 */
#include "lyy_dfssj.h"
#include "lyy_xyj.h"
#include "lyy_device.h"
#include "lyy_tjc.h"

#include "APP_Param.h"
#include "APP_Pluse.h"
#include "APP_Detect.h"
#include "APP_DrillingMode.h"

#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_WIFI.h"
#include "HAL_Uart.h"
#include "HAL_System.h"
#include "HAL_Flash.h"
#include "HAL_Sem.h"

#include "thing_iot.h"
#include "thing_mqtt.h"
#include "thing_event.h"
//#include "sys_packet.h"
//#include "os_inter.h"
#include "lyy_para.h"
//#include "bsp_osInter.h"

#include "utils.h"

static SEM_TYPE dfssj_checkStaus_sem;
DFSSJ_PARMA_DEF dfssj_parma_t;
DFSSJ_PARMA_DEF dfssj_parma_new_t;
DFSSJ_UPDATE_EVENT_DEF dfssj_update_event_t = {0};

PDFSSJ_UPDATE_EVENT_DEF dfssj_get_update_event_infor(void)
{
    return &dfssj_update_event_t;
}


void device_dfssj_netstate_reply(void)
{
    uint8_t buff[10] = {0};
    char *devid = APP_pcGetParamDevId();

    buff[0] = DFSSJ_SUBCMD_NETSTATE;

    if (lyy_mqtt_get_status())
    {
        buff[1] = 1;  
        buff[2] = 1;
    }else{

        buff[1] = 2;
        buff[2] = 2;
    }

    buff[3] = HAL_lGetWifiRssiToCsq(); //使用的CSQ，没有使用rssi
    buff[4] =  devid[1]-  '0';
    buff[5] = ((devid[2] - '0') << 4) + (devid[3] - '0');
    buff[6] = ((devid[4] - '0') << 4) + (devid[5] - '0');
    buff[7] = ((devid[6] - '0') << 4) + (devid[7] - '0');

    device_send_dfssj_packet(buff,8);
}

void device_dfssj_get_all_state(void)
{
    uint8_t buff[10] = {0};
    buff[0] = DFSSJ_SUBCMD_ALL_STATE;

    device_send_dfssj_packet(buff, 1);
}

void device_dfssj_bind_state_info(uint8_t state)
{
    uint8_t buff[10] = {0};
    char *devid = APP_pcGetParamDevId();

    buff[0] = DFSSJ_SUBCMD_BIND_STATA;
    buff[1] = state; // 0 未绑定  1 扫码+刷卡 2 租赁 3 免费

    buff[2] = devid[1] - '0';
    buff[3] = ((devid[2] - '0') << 4) + (devid[3] - '0');
    buff[4] = ((devid[4] - '0') << 4) + (devid[5] - '0');
    buff[5] = ((devid[6] - '0') << 4) + (devid[7] - '0');

    device_send_dfssj_packet(buff, 6);
}

void device_dfssj_all_state_parse(uint8_t *data, uint16_t len)
{
    (void)len;
    uint8_t i;
    dfssj_parma_t.sum = data[1];  //几个出水口

    if (dfssj_parma_t.sum > DFSSJ_LANE_MAX)  dfssj_parma_t.sum = DFSSJ_LANE_MAX;

    for (i = 0; i < dfssj_parma_t.sum; i++)
    {
        dfssj_parma_t.lane[i].state = data[3 + 2 * i];
    }

    lyy_semFreeV2(dfssj_checkStaus_sem); // 解除信号量
}

void device_dfssj_state_infor(void)
{
    char state_string[DFSSJ_STATE_STR_MAX_LEN] = {0};
    uint8_t i;
    uint8_t lane_state;
    uint8_t str_offset = 0;
    uint8_t process_max = (dfssj_parma_t.sum > DFSSJ_LANE_MAX) ? DFSSJ_LANE_MAX : dfssj_parma_t.sum;

    for (i = 0; i < process_max; i++)
    {
        lane_state = dfssj_parma_t.lane[i].state;
        if (lane_state == 1)
        {
            lane_state = 2;
        }
        else if (lane_state == 2)
        {
            lane_state = 1;
        }

        if (str_offset >= DFSSJ_STATE_STR_MAX_LEN - 6)
        {
            break;
        }
        str_offset += sprintf(state_string + str_offset, "%d,0:%d;", i + 1, lane_state);
    }
    // 处理state_string 字符串函数
    cJSON *device_position_status = cJSON_CreateObject();
    if (device_position_status != NULL)
    {
        cJSON_AddStringToObject(device_position_status, "device_position_status", state_string);
        thing_property_post(device_position_status);
    }
}

void device_dfssj_start_end_reply(uint8_t *data, uint16_t len)
{
    (void)len;
    uint8_t buff[10] = {0};
    uint8_t lane_id = data[14]; //哪个出水口
    uint16_t water_quantity=data[12] +data[13]*256 ;   //实际出水数量
    PDFSSJ_UPDATE_EVENT_DEF infor = dfssj_get_update_event_infor();

    // 先应答售水机设备
    memcpy(buff, data, 7); // 包含子命令   唯一码
    buff[7] = 1;  //接受成功
    buff[8] = lane_id; // 出水口
    device_send_dfssj_packet(buff, 9);

    //再把数据发送到服务端
    if (memcmp(g_xlyyHwMark.dfssj_cmd_id[lane_id]+2, data + 3, 4) == 0) // 指令唯一码一样  映射 订单号传给服务端
    {
        if (lane_id<1) lane_id = 1;
        event_post_start_result_dfssj(water_quantity, lane_id, 0, infor->start[lane_id - 1].order);
    }
}

void device_dfssj_time_reply(void)
{
    uint8_t buff[10] = {0};

    buff[0] = DFSSJ_SUBCMD_TIME; 
    buff[1] = 26;   //年
    buff[2] = 4;    //月
    buff[3] = 2;    //日
    buff[4] = 4;    //星期
    buff[5] = 16;    //时
    buff[6] = 9;    //分
    buff[7] = 42;    //秒
    device_send_dfssj_packet(buff, 8);
}

void device_dfssj_fault_reply(void) // 故障数据应答
{
    uint8_t buff[10] = {0};

    buff[0] = DFSSJ_SUBCMD_FAULT;
    device_send_dfssj_packet(buff, 1);
}

/**
 * @brief
 *
 * @param lane_id
 */
void get_dfssj_pay_cmd_id(uint8_t lane_id, uint8_t *cmd_id)
{
    uint8_t i = 0;
    for (i = 0; i < 6; i++)
    {
        g_xlyyHwMark.dfssj_cmd_id[lane_id][i]++;
    }
    memcpy(cmd_id, g_xlyyHwMark.dfssj_cmd_id[lane_id], 6);
}

static void dfssj_device_task(void *arg)
{
    (void)arg;
    uint8_t ret,i;
    uint8_t infor_flag = 0;
    while (1)
    {

        lyy_semNewV2(&dfssj_checkStaus_sem, 0);
        device_dfssj_get_all_state();
        ret = lyy_semWaitTryV2(dfssj_checkStaus_sem, 3000); // 等待主板启动应答
        lyy_semFreeV2(dfssj_checkStaus_sem);
        dfssj_checkStaus_sem = NULL;

        if (ret>0)
        {
            //更新状态
        }else{
            //查询状态应达超时，状态全部置为无效端口
            for (i = 0; i < DFSSJ_LANE_MAX; i++)  //信号超时，全部赋值无效
            {
                dfssj_parma_t.lane[i].state = 0x02; // 0 空闲  1出水中 2无效 3故障
            }
        }

        // 更新上报状态
        if (lyy_mqtt_get_status() == 0x01)
        {
            if (infor_flag == 0)
            {
                infor_flag = 1;
                device_dfssj_state_infor();
            }
            else if (infor_flag == 1)
            {
                for (i = 0; i < dfssj_parma_t.sum; i++)
                {
                    if (dfssj_parma_t.lane[i].state != dfssj_parma_new_t.lane[i].state)
                    {
                        dfssj_parma_new_t.lane[i].state = dfssj_parma_t.lane[i].state;
                        device_dfssj_state_infor();
                        break;
                    }
                }
            }
        }

        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
}

void lyy_device_dfssj_task_init(void)
{
    xTaskCreate(dfssj_device_task,
                "dfssj_device_task",
                1024 * 4, //
                NULL,
                5,
                NULL);
}

/**
 * @brief
 * 支付申请 -- 德锋售水机
 * @param id
 * @return uint8_t
 * > 0 成功
 * =0 失败
 */
uint8_t device_dfssj_pay_apply(uint8_t id)
{
    if (id<1) id = 1;
    if (dfssj_parma_t.lane[id - 1].state == 0)
    {
        return 1;
    }else{
        return 0;
    }
}

/**
 * @brief
 * 售水机 启动
 * @param id  售水机出水口
 * @param number  出水脉冲数量
 * @return uint8_t
 * > 0 成功
 * =0 失败
 */
uint8_t deivce_dfssj_pay_start(uint8_t id, uint16_t number)
{
    DFSSJ_PAY_START_DEF pay = {0};

    pay.sub_cmd = DFSSJ_SUBCMD_PAY_START;
    get_dfssj_pay_cmd_id(id, pay.pay_order);
    pay.method = 0x01;   // 0 先出水后计费  1 按流量出水  2按时间出水

    pay.out_water[0] = number &0x00ff;
    pay.out_water[1] = (number >> 8) & 0xff;
    pay.port = id;    //出水口

    g_xlyyHwMark.dfssj_start_id = id;

    device_send_dfssj_packet(&pay.sub_cmd, sizeof(DFSSJ_PAY_START_DEF));

    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 3000); // 等待主板启动应答
}
/**
 * @brief
 * 德峰售水机串口数据帧处理方法
 * @param package
 * @param len
 */
void device_dfssj_packet_parse(uint8_t *package, uint16_t len)
{
    PDEVICE_DF_PACK_DEF pdevPack = (PDEVICE_DF_PACK_DEF)package;

    lyy_printf_hex(package, len);

    if ((pdevPack->head != 0x5B) || (pdevPack->len != len - 3) || (pdevPack->cmd != 0x0F) || (package[len - 1] != 0xAD))
    {
        return;
    }

    if (package[len - 2] != check_xor_memory((const uint8_t *)package + 1, pdevPack->len))
    {
        return;
    }

    switch (pdevPack->data[0])
    {
    case DFSSJ_SUBCMD_NETSTATE:
        device_dfssj_netstate_reply();
        break;
    case DFSSJ_SUBCMD_PAY_START:
         {
             if (g_xlyyHwMark.ucPaySem && g_xlyyHwMark.dfssj_start_id == pdevPack->data[8])
             {

                 if (pdevPack->data[7] == 1)
                 {
                     g_xlyyHwMark.startResult = 1;
                 }
                 else
                 {

                     g_xlyyHwMark.startResult = 0;
                 }

                 if (g_xlyyHwMark.ucPaySem)
                 {
                     xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                     g_xlyyHwMark.ucPaySem = 0x00;
                 }
             }
         }
        break;
    case DFSSJ_SUBCMD_START_END:
        device_dfssj_start_end_reply(pdevPack->data, len - 5); // 出水结束应答  和  出水结束结果上报
        break;
    case DFSSJ_SUBCMD_ONE_STATE: //
        break;
    case DFSSJ_SUBCMD_ALL_STATE: //
        device_dfssj_all_state_parse(pdevPack->data, len - 5);
        break;
    case DFSSJ_SUBCMD_SET_PARAM: //
        break;
    case DFSSJ_SUBCMD_GET_PARAM: //
        break;
    case DFSSJ_SUBCMD_SWIPING_CARD: //
        break;
    case DFSSJ_SUBCMD_SWIPING_CARD_END: //
        break;
    case DFSSJ_SUBCMD_TIME: //
        device_dfssj_time_reply();
        break;
    case DFSSJ_SUBCMD_WATER_TEMP: //
        break;
    case DFSSJ_SUBCMD_GET_CONSUMABLE: //
        break;
    case DFSSJ_SUBCMD_SET_CONSUMABLE: //
        break;
    case DFSSJ_SUBCMD_GET_IC: //
        break;
    case DFSSJ_SUBCMD_FAULT: //
        device_dfssj_fault_reply();
        break;
    case DFSSJ_SUBCMD_GET_PARAM_2: //
        break;
    case DFSSJ_SUBCMD_SET_PARAM_2: //
        break;
    default:
        break;
    }
}



