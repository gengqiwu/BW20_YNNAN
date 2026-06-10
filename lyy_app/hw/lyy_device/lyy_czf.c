/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:51
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 16:46:18
 * @Description: 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "APP_Param.h"
#include "APP_DrillingMode.h"

#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_WIFI.h"
#include "HAL_Uart.h"
#include "HAL_System.h"
#include "HAL_Flash.h"

#include "USER_Config.h"

#include "thing_init.h"
#include "thing_iot.h"
#include "thing_mqtt.h"
#include "thing_event.h"
#include "thing_property.h"

#include "lyy_device.h"
#include "lyy_czf.h"
#include "lyy_tjc.h"
#include "lyy_para.h"

#include "utils.h"
#include "cJSON.h"

char device_packet_czf_send(PDEVICE_PACK_CZF_DEF pack, uint16_t inlen)
{
    uint8_t *tp = &pack->head;
    uint16_t len = inlen + 3;
    pack->head = 0xAA;
    pack->end = 0xDD;
    pack->cmd = 0xD7;

    pack->check = check_xor_memory(&(pack->len[0]), inlen);
    tp[len - 2] = pack->check;
    tp[len - 1] = pack->end;

    HAL_vUartSend(HAL_UART_MAIN_BOARD, tp, len, 500);

    return len;
}

/**
 * @brief
 * 带数据区长度
 * @param cmd
 * @param data
 * @param dataLen
 */
void device_packet_czf_data_length(uint8_t sud_cmd, uint8_t *data, uint16_t dataLen)
{
    uint16_t packLen;
    DEVICE_PACK_CZF_DEF tDevPack;

    if (dataLen > sizeof(tDevPack.data))
        return;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tDevPack.sud_cmd = sud_cmd;

    packLen = dataLen + 3;

    data_little16Toarray(packLen, &(tDevPack.len[0]));

    if ((dataLen > 0) && (data != NULL))
    {
        memcpy(tDevPack.data, data, dataLen);
    }
    device_packet_czf_send(&tDevPack, dataLen + 4);
}




void device_czf_qrcode_apply_reply(void)
{
    uint8_t reply[1] = {0x01};

    if (lyy_mqtt_get_status())
    {
        reply[0] = 0x01;
    }
    else
    {
        reply[0] = 0x00;
    }
    device_packet_czf_data_length(0x01, reply, sizeof(reply));
}

void device_czf_qrcode_apply(uint8_t *data, uint16_t len)
{
    (void) len;
    uint32_t czf_amount;
    cJSON *value = NULL;
    char unique[7] = {0};

    PDEVICE_GET_QRCODE_CZF_DEF qrcode_czf = (PDEVICE_GET_QRCODE_CZF_DEF)data;
    czf_amount = (qrcode_czf->amount[0]) | (qrcode_czf->amount[1] << 8) | (qrcode_czf->amount[2] << 16) | (qrcode_czf->amount[3] << 24);
    /*处理商户和钱包编号，传给服务端*/
    value = cJSON_CreateObject();
    if (value != NULL)
    {

        cJSON_AddNumberToObject(value, "commodity_amount", (double)czf_amount / 100.0);      // 商品金额
        cJSON_AddStringToObject(value, "pay_channel_code", (char *)qrcode_czf->wallet);      // 支付渠道（钱包能力编码）
        cJSON_AddStringToObject(value, "pay_channel_number", (char *)qrcode_czf->channelNumber); // 支付渠道编号（商户渠道编码）

        memcpy(unique, (char *)qrcode_czf->unique,6);  //JSON填充数据 必须0结尾
        cJSON_AddStringToObject(value, "unique_code", unique); // 请求的唯一标识

        thing_event_post(value, "amount_get_qrcode");  //金额获取二维码
    }
        
    device_czf_qrcode_apply_reply();

}

void device_czf_start_status_reply(void)
{
    uint8_t reply[1] = {0x01};

    device_packet_czf_data_length(0x04, reply, sizeof(reply));
}

void device_czf_start_status(uint8_t *data, uint16_t len)
{
    (void) len;

    uint8_t order[64] = {0};
    cJSON *value = NULL;

    if (data[2]>63)
        return;

    memcpy(order, &(data[3]), data[2]);

    value = cJSON_CreateObject();
    if (value != NULL)
    {
        if (data[1])
        {
            cJSON_AddNumberToObject(value, "start_status", 1);    //启动成功
        }else{

            cJSON_AddNumberToObject(value, "start_status", 2);   //启动失败
        }

        cJSON_AddStringToObject(value, "pay_order", (char *)order); // 启动订单号

        thing_event_post(value, "pay_start_status");              //支付启动状态上报
    }

    device_czf_start_status_reply();
}

void device_czf_get_pay_channel(uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;
    uint8_t i;
    uint8_t channel_data[1225] = {0};

    PAY_PARAM_DEF *pay_channel_t = get_pay_param();
    DEVICE_PAY_CHANNEL_CZF_DEF pay_channel_num[4] = {0};

    if (tjc_infor_t.get_pay_channel_flag != 0x01)
    {
        return;
    }

    channel_data[0] = pay_channel_t->chnl_sum;

    for (i = 0; i < pay_channel_t->chnl_sum; i++)
    {
        pay_channel_num[i].number = i + 1;
        memcpy(pay_channel_num[i].channel_number, pay_channel_t->channel[i].channel_number, 64);
        memcpy(pay_channel_num[i].code, pay_channel_t->channel[i].code, 32);
        memcpy(pay_channel_num[i].pay_type, pay_channel_t->channel[i].pay_type, 32);

        memcpy(&(channel_data[i * sizeof(pay_channel_num[0]) + 1]), &(pay_channel_num[i].number), sizeof(pay_channel_num[0]));
    }
    
    

    device_packet_czf_data_length(0x06, channel_data, (sizeof(pay_channel_num[0])*channel_data[0] + 1));
}

void device_czf_pos_clear(uint8_t *data, uint16_t len)
{
    (void)len;

    uint8_t order[64] = {0};
    cJSON *value = NULL;

    if (data[2] > 63)
        return;

    memcpy(order, &(data[3]), data[2]);

    value = cJSON_CreateObject();
    if (value != NULL)
    {
        if (data[1]== 0x02)
        {
            cJSON_AddNumberToObject(value, "create_resul", 2); // 启动成功
        }
        else
        {

            cJSON_AddNumberToObject(value, "create_result", 0); // 
        }

        cJSON_AddStringToObject(value, "order_no", (char *)order); // 启动订单号
        cJSON_AddStringToObject(value, "transaction_number","");  //传入空字符串
        cJSON_AddStringToObject(value, "create_failure_reason", ""); // 传入空字符串

        thing_event_post(value, "pos_order_create_result_event"); // 支付启动状态上报
    }

}

void device_czf_net_state(uint8_t *data, uint16_t len)
{
    (void)len;
    (void)data;

    uint8_t reply[1] = {0x01};

    if (lyy_mqtt_get_status())
    {
        reply[0] = 0x03;
    }
    else
    {
        reply[0] = 0x00;
    }
    device_packet_czf_data_length(0x08, reply, sizeof(reply));
}
/**
 * @brief
 * 纯支付数据处理
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_czf_packet_parse(uint8_t subCmd, uint8_t *data, uint16_t len)
{
    (void)data;
    (void)len;

    if (data == NULL)
        return;

    LYY_LOGD("device_czf_packet_parse subCmd:%02X", subCmd);

    switch (subCmd)
    {
    case 0x01: // 主板带价格，支付二维码申请
        device_czf_qrcode_apply(data, len);
        break;
    case 0x02: // 主板接收支付二维码的应答
        break;
    case 0x03: // 主板接收QR支付成功的应答
        break;
    case 0x04: // 主板返回机器启动状态
        device_czf_start_status(data, len);
        break;
    case 0x05: // 主板获取支付渠道(json方式)
        device_czf_get_pay_channel(data, len);
        break;
    case 0x06: // 主板获取支付渠道(hex方式)
        device_czf_get_pay_channel(data, len);
        break;
    case 0x07: // 主板取消POS支付
        device_czf_pos_clear(data, len);
        break;
    case 0x08: // 主板取消POS支付
        device_czf_net_state(data, len);
        break;
    default:
        break;
    }
}