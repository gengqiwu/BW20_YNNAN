/**
 * @file lyy_start.c
 * @brief 支付请求事件提交、纸钞主板申请及启动即时应答衔接。
 */
/*
 * @author: e.he
 * @Date: 2022-10-17 11:28:37
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-08 18:38:22
 * @Description: 
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "utils.h"

#include "thing_init.h"
#include "thing_event.h"
#include "thing_service.h"

#include "APP_Event.h"
#include "APP_Param.h"

#include "HAL_Log.h"
#include "HAL_System.h"


#include "lyy_tjc.h"
#include "lyy_zcj.h"
#include "lyy_para.h"
#include "lyy_start.h"
#include "lyy_device.h"

GET_PAY_URL_PARAM_DEF pay_url_param_t;  //获取动态二维码参数


/**
 * @brief Get the url uinque code object
 * 
 * @return char* 
 */
char *get_py_url_uinque_code(uint8_t id)
{
    char cmd_id[6] = {0x00,};
    vRandomGet(cmd_id, sizeof(cmd_id));
    memset(pay_url_param_t.uinque[id], 0, sizeof(pay_url_param_t.uinque[id]));
    utils_lhexToString(pay_url_param_t.uinque[id], (const unsigned char*)cmd_id, sizeof(cmd_id));
    LYY_LOGD("get_url_uinque_code :%s",pay_url_param_t.uinque[id]);
    return pay_url_param_t.uinque[id];
}
/**
 * @brief 
 * 
 * @param uinque_id 
 * @param lane_id 
 * @return int 
 * TRUE ：不一样
 * FALSE : 一样
 */
int cmp_pay_url_uinque_code(uint8_t id, char *uinque_id)
{
    LYY_LOGD("cmp_pay_url_uinque_code");
    LYY_LOGD("get_url_id :%s",pay_url_param_t.uinque[id]);
    LYY_LOGD("uinque_id :%s",uinque_id);
    if(strlen(pay_url_param_t.uinque[id]) != strlen(uinque_id)) return 0x01;  //不一样
    if(strstr(pay_url_param_t.uinque[id], uinque_id)==NULL) return 0x01; //不一样
    return 0x00;
}

/**
 * @brief 
 * 
 * @param id  //仓号  0 默认的使用与通用
 * @param number  //从0 开始
 */
void pay_url_wallet_number_set(uint8_t number)
{
    pay_url_param_t.wallet_num = number;
}

/**
 * @brief 
 * 
 * @param id  //仓好 0 默认的使用与通用
 * @return uint8_t 
 */
uint8_t pay_url_package_id_get(void)
{
    return pay_url_param_t.package_id;
}
/**
 * @brief 
 * 
 * @param id  //仓号  0 默认的使用与通用
 * @param number  //从0 开始
 */
void pay_url_package_id_set(uint8_t package_id)
{
    pay_url_param_t.package_id = package_id;
}

/**
 * @brief 
 * 
 * @return uint8_t 
 */
uint8_t pay_url_wallet_number_get(void)
{
    return pay_url_param_t.wallet_num;
}

#if 0
/**
 * @brief 
 * 
 * @return uint8_t 
 */
uint8_t pay_url_wallet_number_get(void)
{
    return pay_url_param_t.wallet_num;
}


#endif

/**
 * @brief 消费支付申请事件，等待主板申请应答并回复云端。
 * @param[in] pucData 指向 HW_PAY_APPLY 的事件缓冲区，调用方须保证结构完整。
 * @param[in] ulLen 缓冲区长度；实现仅检查非零，不检查完整结构长度。
 * @note 仅脉冲、电平、纸钞类型发起请求；命令等待失败使用 timeout，其余使用主板申请结果。
 */
void lyy_start_pay_apply(uint8_t *pucData, uint32_t ulLen)
{

    if(!pucData || !ulLen) {
        return ;
    }

    HW_PAY_APPLY *pxPayApply = (HW_PAY_APPLY *)pucData;
    int ret;

    switch (get_dev_type_code())
    {
    case DEVICE_TYPE_PULSE:
    case DEVICE_TYPE_LEVEL:
    case DEVICE_TYPE_UART_NOTE:
        ret = zcj_cmd_pay_apply(pxPayApply->uniqueid) > 0 ?
              g_xlyyHwMark.applyResult : PAY_START_RESULT_TIMEOUT;
        thing_service_reply_pay_apply_note(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        break;
    default: break;
    }

}


/**
 * @brief 消费启动事件并发送即时接单状态应答。
 * @param[in] pucData 至少包含一个 HW_PAY_APPLY 的事件缓冲区。
 * @param[in] ulLen 缓冲区字节数，不足结构大小时忽略。
 * @note 脉冲、电平类型实际下发 NOTE_CHANNLE_PULSE，云端应答仍携带原事件通道。
 * @note 主板即时结果 0/1/5 映射为 startup_failed/start_normally/timeout，其余为 other；不是 0x07 最终执行事件。
 */
void lyy_start_pay_start(uint8_t *pucData, uint32_t ulLen)
{

    if (pucData == NULL || ulLen < sizeof(HW_PAY_APPLY)) return;
    HW_PAY_APPLY *pxPayApply = (HW_PAY_APPLY *)pucData;
    int type = get_dev_type_code();
    int ret;
    uint8_t channel;
    if (type != DEVICE_TYPE_PULSE && type != DEVICE_TYPE_LEVEL && type != DEVICE_TYPE_UART_NOTE) return;
    channel = type != DEVICE_TYPE_UART_NOTE ? NOTE_CHANNLE_PULSE : pxPayApply->channel;
    g_xlyyHwMark.startResult = PAY_START_RESULT_TIMEOUT;
    zcj_cmd_pay_start(pxPayApply->uniqueid, channel, pxPayApply->number);
    if (g_xlyyHwMark.startResult == 0) ret = PAY_START_RESULT_FAIL_DBJ;
    else if (g_xlyyHwMark.startResult == 1) ret = PAY_START_RESULT_SUCCESS_DBJ;
    else if (g_xlyyHwMark.startResult == 5) ret = PAY_START_RESULT_TIMEOUT;
    else ret = PAY_START_RESULT_OTHER;
    thing_service_reply_pay_start_note(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id, pxPayApply->channel);
}


/**
 * @brief 复制支付参数并尝试提交到启动事件队列。
 * @param[in] type START_TYPE_PAY_APPLY 或 START_TYPE_PAY_START；其他值不提交。
 * @param[in] num 启动数量，原样保存。
 * @param[in] order 订单号，最多复制 63 字节并保留结尾零。
 * @param[in] x 主板通道，保存到事件的 channel 字段。
 * @param[in] y 保留兼容参数，未使用。
 * @param[in] id 云端请求标识，最多复制 63 字节。
 * @retval 0 order 或 id 为 NULL。
 * @retval 1 其余情况，包括不支持的 type。
 * @warning 不读取 APP_vEventSendBuff 的投递结果；返回 1 仅表示入队尝试结束，不保证投递、接单或执行成功。
 */
int api_std_start_entryQue(uint8_t type,uint16_t num,char *order , uint16_t x, uint16_t y, char *id)
{


    if(!order || !id) {

        return 0;
    }

    HW_PAY_APPLY pxPayApply;

    memset((char *)&pxPayApply, 0x00, sizeof(HW_PAY_APPLY));

    uint32_t len = strlen(order) >= sizeof(pxPayApply.uniqueid) ?
                                sizeof(pxPayApply.uniqueid) - 1 : strlen(order);

    memcpy(pxPayApply.uniqueid, order, len);

    len = strlen(id) >= sizeof(pxPayApply.pay_apply_id) ?
                                sizeof(pxPayApply.pay_apply_id) - 1 : strlen(id);

    memcpy(pxPayApply.pay_apply_id, id, len);

    pxPayApply.number = num;
    pxPayApply.channel = x;
    (void)y;

    switch(type)
    {

        case START_TYPE_PAY_APPLY   :{

            APP_vEventSendBuff( eEventStart, 
                                lyy_start_pay_apply, 
                                (uint8_t *)&pxPayApply, 
                                sizeof(HW_PAY_APPLY));
        }break;


        case START_TYPE_PAY_START  :{

            APP_vEventSendBuff( eEventStart, 
                                lyy_start_pay_start, 
                                (uint8_t *)&pxPayApply, 
                                sizeof(HW_PAY_APPLY));
        }break;


        default :
            break;

    }


    return 1;
}




