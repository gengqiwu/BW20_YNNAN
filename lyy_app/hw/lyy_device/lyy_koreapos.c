/*
 * @author: e.he
 * @Date: 2024-07-10 16:29:48
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-10-09 16:31:10
 * @Description:
 *
 */
#include "lyy_koreapos.h"
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
#include "thing_event.h"

#include "thing_iot.h"
#include "thing_mqtt.h"
// #include "sys_packet.h"
// #include "os_inter.h"
#include "lyy_para.h"
// #include "bsp_osInter.h"
#include "utils.h"

KOREAPOS_PARAM_DEF koreapos_param_t;

/**
 * @brief
 * 发送数据到
 * @param objName 对象名称
 * @param txt 对象内容
 */
void lyy_koreapos_cmd_send(uint8_t *data, uint16_t len)
{
    HAL_vUartSend(HAL_UART_MAIN_BOARD, data, len, 5000);
}

void lyy_koreapos_pay_success_ACK(void)   //支付成功后的应答
{
   uint8_t cmdBuff[5]={0XAA,0XE0,0X01,0X00,0X4B,};

   lyy_koreapos_cmd_send(cmdBuff, sizeof(cmdBuff));
}

void lyy_koreapos_payment(uint32_t amount,uint16_t timeout)  //支付金额 最大六个9    支付超时时间 最大三个 999
{
    uint8_t cmdBuff[13] = {0XAA, 0X00, 0X09, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00, 0X00};

    if (amount > 999999 || amount<=0)
    {
        return;
    }

    if (timeout > 999 || timeout <= 0)
    {
        return;
    }

    char tmp[10];

    sprintf(tmp, "%06u", (unsigned int)amount);
    memcpy(&cmdBuff[3], tmp, 6);

    memset(tmp, 0, sizeof(tmp));
    sprintf(tmp, "%03u", (unsigned int)timeout);
    memcpy(&cmdBuff[9], tmp, 3);

    cmdBuff[12] = check_xor_memory(cmdBuff, sizeof(cmdBuff)-1);
    lyy_koreapos_cmd_send(cmdBuff, sizeof(cmdBuff));
}

void lyy_koreapos_cancel_payment(void)
{
   uint8_t cmdBuff[4]={0XAA,0X01,0X00,0X00,};

   cmdBuff[3] = check_xor_memory(cmdBuff, sizeof(cmdBuff) - 1);
   lyy_koreapos_cmd_send(cmdBuff, sizeof(cmdBuff));
}

/**
 * @brief
 *
 * @param package
 * @param len
 */
void koreapos_packet_parse(uint8_t *package, uint16_t len)
{

    if (package[0] != 0xaa || package[len - 1] != check_xor_memory(package, len - 1))
    {
        return;
    }

    if (package[1] == 0xE0)
    {
        if (package[3] == 0x00)
        {
            //发起支付成功
            koreapos_param_t.pay_flag = 1;  //标记发起支付成功
            //收到应答，才赋值交易流水号
            memset(koreapos_param_t.transaction_number, 0x00, sizeof(koreapos_param_t.transaction_number));
            strcpy(koreapos_param_t.transaction_number, koreapos_param_t.transaction_number_new);

            event_post_pos_order_create_result(koreapos_param_t.transaction_number,1);  //发起支付成功
        }
       else
       {
           //发起支付失败了
           koreapos_param_t.pay_flag = 0;                                              // 回归默认状态
           event_post_pos_order_create_result(koreapos_param_t.transaction_number,2);  //发起支付失败
           
       }
    }
    else if (package[1] == 0x80)
    {
        koreapos_param_t.pay_flag = 0; // 回归默认状态
        if (package[9] == 0x00)  //支付成功
        {
            //回应后台支付成功
            event_post_pos_order_detail(koreapos_param_t.transaction_number, 1);
            //应答POS机
            lyy_koreapos_pay_success_ACK();
        }  
        else if (package[9] == 0x01)  //支付超时
        {
            // 回应后台支付超时
            event_post_pos_order_detail(koreapos_param_t.transaction_number, 2);
            // 应答POS机
            lyy_koreapos_pay_success_ACK();
        }
        else if (package[9] == 0x02)   //支付取消完成
        {
            // 回应后台支付取消完成
            event_post_pos_order_detail(koreapos_param_t.transaction_number, 2);
            // 应答POS机
            lyy_koreapos_pay_success_ACK();
        }
        else if (package[9] == 0x03)   //支付错误
        {
            // 回应后台支付错误
            event_post_pos_order_detail(koreapos_param_t.transaction_number, 2);
            // 应答POS机
            lyy_koreapos_pay_success_ACK();
        }
        else if (package[9] == 0x04)  //支付终止
        {
            // 回应后台支付终止
            event_post_pos_order_detail(koreapos_param_t.transaction_number, 2);
            // 应答POS机
            lyy_koreapos_pay_success_ACK();
        }
        else
        {
            // 应答POS机
            lyy_koreapos_pay_success_ACK();
        }
    }
}



