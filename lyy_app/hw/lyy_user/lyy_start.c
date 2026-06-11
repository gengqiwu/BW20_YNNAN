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

#include "APP_Pluse.h"
#include "APP_Event.h"
#include "APP_Param.h"

#include "HAL_Log.h"
#include "HAL_System.h"


#include "lyy_tjc.h"
#include "lyy_ndj.h"
#include "lyy_amy.h"
#include "lyy_wwj.h"
#include "lyy_dbj.h"
#include "lyy_adh814e.h"
#include "lyy_yunanshj.h"
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

void lyy_start_pay_apply(uint8_t *pucData, uint32_t ulLen)
{

    if(!pucData || !ulLen) {
        return ;
    }

    HW_PAY_APPLY *pxPayApply = (HW_PAY_APPLY *)pucData;
    int ret;

    switch(get_dev_type_code())
    {

        case DEVICE_TYPE_PULSE:
        case DEVICE_TYPE_LEVEL:
        
            thing_service_reply_pay_apply(pxPayApply->uniqueid, PAY_START_RESULT_SUCCESS, pxPayApply->pay_apply_id);
        break;

        case DEVICE_TYPE_UART_WWJ: // 娃娃机
        {

            g_xlyyHwMark.ucPaySem = 0x01;
            ret = (device_wwj_pay_apply() > 0) ? PAY_START_RESULT_SUCCESS : PAY_START_RESULT_TIMEOUT;
            g_xlyyHwMark.ucPaySem = 0x00;

            thing_service_reply_pay_apply(pxPayApply->uniqueid ,ret, pxPayApply->pay_apply_id);

        }break;

        case DEVICE_TYPE_UART_DBJ:  //兑币机
        {

            g_xlyyHwMark.ucPaySem = 0x01;
            ret = (device_dbj_pay_apply() > 0) ? g_xlyyHwMark.applyResult : PAY_START_RESULT_TIMEOUT; 
            g_xlyyHwMark.ucPaySem = 0x00;
            
            thing_service_reply_pay_apply_dbj(pxPayApply->uniqueid ,ret, pxPayApply->pay_apply_id);    

        }break;

        case DEVICE_TYPE_UART_NDJ:  //扭蛋机
        {
            //ndj_add
            int reuslt = 0;
            g_xlyyHwMark.ucPaySem = 0x01;
            ret = (device_ndj_pay_apply(1,pxPayApply->x+1) > 0) ? g_xlyyHwMark.applyResult:PAY_START_RESULT_TIMEOUT;
            g_xlyyHwMark.ucPaySem = 0x00;
            if(ret == 0) reuslt = 1; //主办协议0是成功 1是失败
            else if(ret == 1) reuslt = 0;
            else reuslt = ret ;
            LYY_LOGD("result :%d ret:%d",reuslt,ret);
            thing_service_reply_pay_apply_ndj(pxPayApply->x+1,pxPayApply->uniqueid,reuslt,pxPayApply->pay_apply_id);  
        }break;

        case DEVICE_TYPE_UART_AMY:  //按摩椅
        {   

            g_xlyyHwMark.ucPaySem = 0x01;
            ret = (device_amy_pay_apply() > 0) ? g_xlyyHwMark.applyResult : PAY_START_RESULT_TIMEOUT; //
            g_xlyyHwMark.ucPaySem = 0x00;
            if(ret == 0 || ret == 1)
                thing_service_reply_pay_apply(pxPayApply->uniqueid,1,pxPayApply->pay_apply_id);  // 00空闲 或者 01运行中
            else
                thing_service_reply_pay_apply(pxPayApply->uniqueid,0,pxPayApply->pay_apply_id);   // 其他状态为失败
        }break;

        case DEVICE_TYPE_UART_SHJ: // 售货机
        {

            ret = (device_adh814e_pay_apply() > 0) ? g_xlyyHwMark.applyResult : PAY_START_RESULT_TIMEOUT; //

            thing_service_reply_pay_apply(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id); // 1成功，0超时
        }
        break;

        case DEVICE_TYPE_UART_YUNANSHJ: // 宇楠售货机
        {
            g_xlyyHwMark.ucPaySem = 0x01;//当前有支付申请在等待结果
            ret = (device_yunanshj_pay_apply() > 0) ? g_xlyyHwMark.applyResult : PAY_START_RESULT_TIMEOUT;
            g_xlyyHwMark.ucPaySem = 0x00;//清除标志，表示支付申请流程已结束
            thing_service_reply_pay_apply(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;

        default:break;
    }

}


void lyy_start_pay_start(uint8_t *pucData, uint32_t ulLen)
{

    (void)ulLen;

    char start_ret = -1;
    int result = 0;
    int ret = 0;
    int refund = 0;
    uint8_t i = 24;
    uint8_t gift_number = 0;

    HW_PAY_APPLY *pxPayApply = (HW_PAY_APPLY *)pucData;
    
    switch(get_dev_type_code())
    {
        case DEVICE_TYPE_LEVEL:         
        case DEVICE_TYPE_PULSE:   
            (START_TYPE_LEVEL == APP_pxlGetParamePulse()->ulPulsetype) ?
                    (APP_vPulseStart(APP_COIN_LEVEL, pxPayApply->number)):(APP_vPulseStart(APP_COIN_PLUSE, pxPayApply->number));
        break;

        case DEVICE_TYPE_UART_WWJ: 
        {

            g_xlyyHwMark.startResult = 0;

            g_xlyyHwMark.ucPaySem = 0x01;
            start_ret = device_wwj_pay_start(pxPayApply->number, pxPayApply->number);
            g_xlyyHwMark.ucPaySem = 0x00;
            
            LYY_LOGD("start_ret[wwj]:%d",start_ret);

            // if(start_ret>0)//接收到成功应答
            // {
            //     result = (g_xlyyHwMark.startResult > 0) ? pxPayApply->number : 0;
            // }

            ret = g_xlyyHwMark.startResult > 0 ? 0x01 : 0x00;
            thing_service_reply_pay_wwj(pxPayApply->uniqueid, ret, (char *)pxPayApply->pay_apply_id);
  
            vTaskDelay(200 / portTICK_PERIOD_MS);
            if(g_xlyyHwMark.startResult == 0) 
                refund = pxPayApply->number;
            else 
                refund = 0;

            event_post_start_result_wwj(0,g_xlyyHwMark.startResult,refund,pxPayApply->uniqueid); //上报启动结果

        }break;


        case DEVICE_TYPE_UART_DBJ: 
        {

            g_xlyyHwMark.startResult = 0;

            g_xlyyHwMark.ucPaySem = 0x01;
            start_ret = device_dbj_pay_start(0, pxPayApply->number);
            g_xlyyHwMark.ucPaySem = 0x00;

            LYY_LOGD("start_ret[dbj]:%d",start_ret);

            // if(start_ret>0)//接收到成功应答
            // {
            //     result = (g_xlyyHwMark.startResult == 0x01) ? pxPayApply->number : 0;
            // }

            if(g_xlyyHwMark.startResult == 0) 
                ret =  PAY_START_RESULT_FAIL_DBJ;
            else if(g_xlyyHwMark.startResult == 1) 
                ret =  PAY_START_RESULT_SUCCESS_DBJ;
            else if(g_xlyyHwMark.startResult == 2) 
                ret =  PAY_START_RESULT_STARTING;
            else  ret =  PAY_START_RESULT_FAIL_DBJ;

            thing_service_reply_pay_start_dbj(pxPayApply->uniqueid ,ret, pxPayApply->pay_apply_id, pxPayApply->number , 0);

        }break;

        case DEVICE_TYPE_UART_NDJ: 
        {  
            PNDJ_UPDATE_EVENT_DEF infor = get_update_event_infor();
            memset(infor->start[pxPayApply->x].order,0,sizeof(infor->start[pxPayApply->x].order));
            strcpy(infor->start[pxPayApply->x].order,pxPayApply->uniqueid); //缓存订单号
            infor->start[pxPayApply->x].flag = false; //清零上报标识
            g_xlyyHwMark.ucPaySem = 0x01;
            result = (deivce_ndj_pay_start(1,pxPayApply->x+1,pxPayApply->number)>0) ? g_xlyyHwMark.startResult:0;
            g_xlyyHwMark.ucPaySem = 0x00;
            
            LYY_LOGD("result[ndj]:%d",result);
            thing_service_reply_pay_start_ndj(pxPayApply->x+1,pxPayApply->uniqueid,result,pxPayApply->pay_apply_id); 
        }break;

        case DEVICE_TYPE_UART_AMY: 
        {

            g_xlyyHwMark.startResult = 0;

            g_xlyyHwMark.ucPaySem = 0x01;
            start_ret = device_amy_pay_start(pxPayApply->number);
            g_xlyyHwMark.ucPaySem = 0x00;

            LYY_LOGD("start_ret[amy]:%d",start_ret);

            if(start_ret > 0)//接收到成功应答
            {
                if(g_xlyyHwMark.startResult == 0x01) //成功
                {

                    lyy_tjc_page_amy_time_set(pxPayApply->number * 60); //返回成功才倒计时
                    g_tAMYState.work_state = true;
                }
            }

            ret = (g_xlyyHwMark.startResult == 1) ? 1:0;  
            thing_service_reply_pay_start_amy(pxPayApply->uniqueid ,ret, pxPayApply->pay_apply_id);

        }break;
        case DEVICE_TYPE_UART_SHJ:  //售货机
        {
            g_xlyyHwMark.startResult = 0;

            g_xlyyHwMark.ucPaySem = 0x01;
            start_ret = (device_adh814e_pay_statr(pxPayApply->number) > 0) ? g_xlyyHwMark.startResult : PAY_START_RESULT_TIMEOUT;
            g_xlyyHwMark.ucPaySem = 0x00;

            if (start_ret!= 1)   //不等于1，标识启动失败，直接上报结果
            {
                LYY_LOGD("start_ret[SHJ111]:%d",start_ret);
                thing_service_reply_pay_start_shj(pxPayApply->uniqueid,
                                                  shj_pay_start_t.order_item[0].itemOrder,
                                                  shj_pay_start_t.order_item[0].positionId,
                                                  shj_pay_start_t.order_item[0].positionX,
                                                  shj_pay_start_t.order_item[0].positionY,
                                                  start_ret,
                                                  0,
                                                  shj_pay_start_t.order_item[0].stock,
                                                  pxPayApply->pay_apply_id);
            }
            else
            {
                while (--i)
                {
                    vTaskDelay(800 / portTICK_PERIOD_MS);
                    if (checkSta_flag == 1)  //防止通信失败
                    {
                        if (state_814_t.state == 1 && state_814_t.result == 0)
                        {
                            continue;
                        }

                        if (state_814_t.result != 0)
                        {
                            // 出货失败。

                            if (state_814_t.result == 0x04)
                            {
                                //start_ret = 10;  //其他 other  有调货检测的需要开启
                                start_ret = 1;  //  无掉货检测，直接成功
                                gift_number = 1; //出礼固定为1
                            }
                            else
                            {
                                start_ret = 2;
                                gift_number = 0;
                            }

                            thing_service_reply_pay_start_shj(pxPayApply->uniqueid,
                                                              shj_pay_start_t.order_item[0].itemOrder,
                                                              shj_pay_start_t.order_item[0].positionId,
                                                              shj_pay_start_t.order_item[0].positionX,
                                                              shj_pay_start_t.order_item[0].positionY,
                                                              start_ret,
                                                              gift_number,
                                                              (shj_pay_start_t.order_item[0].stock - gift_number),
                                                              pxPayApply->pay_apply_id);
                            break;
                        }

                        if (state_814_t.result == 0)
                        {
                            //出货成功

                            thing_service_reply_pay_start_shj(pxPayApply->uniqueid,
                                                              shj_pay_start_t.order_item[0].itemOrder,
                                                              shj_pay_start_t.order_item[0].positionId,
                                                              shj_pay_start_t.order_item[0].positionX,
                                                              shj_pay_start_t.order_item[0].positionY,
                                                              1,
                                                              1,
                                                              (shj_pay_start_t.order_item[0].stock-1),
                                                              pxPayApply->pay_apply_id);
                            break;
                        }
                    }
                    
                } 
                
                if(i<=0)  //出货超，表示出货失败
                {
                    LYY_LOGD("start_ret[SHJ222]:%d", start_ret);
                    start_ret = 5;  //超时
                    thing_service_reply_pay_start_shj(pxPayApply->uniqueid,
                                                      shj_pay_start_t.order_item[0].itemOrder,
                                                      shj_pay_start_t.order_item[0].positionId,
                                                      shj_pay_start_t.order_item[0].positionX,
                                                      shj_pay_start_t.order_item[0].positionY,
                                                      start_ret,
                                                      0,
                                                      shj_pay_start_t.order_item[0].stock,
                                                      pxPayApply->pay_apply_id);
                }
                 
            }
            adh814e_cmd_ack();
            shj_pay_start_t.startflag = 0;   //更新标识，可以进行下次售货机支付启动
        }
        break;

        case DEVICE_TYPE_UART_YUNANSHJ:  //宇楠售货机
        {
            g_xlyyHwMark.startResult = 0;
            g_xlyyHwMark.ucPaySem = 0x01;
            start_ret = (device_yunanshj_pay_statr() > 0) ? g_xlyyHwMark.startResult : PAY_START_RESULT_TIMEOUT;
            g_xlyyHwMark.ucPaySem = 0x00;

            LYY_LOGD("start_ret[YUNANSHJ]:%d", start_ret);

            // 上报出货结果
            thing_service_reply_pay_start_shj(pxPayApply->uniqueid,
                                              shj_pay_start_t.order_item[0].itemOrder,
                                              shj_pay_start_t.order_item[0].positionId,
                                              shj_pay_start_t.order_item[0].positionX,
                                              shj_pay_start_t.order_item[0].positionY,
                                              start_ret,
                                              (start_ret == 0) ? 1 : 0,  // 成功出货数
                                              shj_pay_start_t.order_item[0].stock,
                                              pxPayApply->pay_apply_id);

            shj_pay_start_t.startflag = 0;   //更新标识，可以进行下次售货机支付启动
        }
        break;

        default:break;
    }          
}


int api_std_start_entryQue(uint8_t type,uint16_t num,char *order , uint16_t x, uint16_t y, char *id)
{


    if(!order || !id) {

        return 0;
    }

    HW_PAY_APPLY pxPayApply;

    memset((char *)&pxPayApply, 0x00, sizeof(HW_PAY_APPLY));

    uint32_t len = strlen(order) > sizeof(pxPayApply.uniqueid) ? 
                                sizeof(pxPayApply.uniqueid) : strlen(order);

    memcpy(pxPayApply.uniqueid, order, len);

    len = strlen(id) > sizeof(pxPayApply.pay_apply_id) ? 
                                sizeof(pxPayApply.pay_apply_id) : strlen(id);

    memcpy(pxPayApply.pay_apply_id, id, len);

    pxPayApply.number = num;
    pxPayApply.x = x;
    pxPayApply.y = y;

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

        case START_TYPE_NDJ_LANE_INFOR_GET  :{

            deivce_ndj_lane_infor_get(1, pxPayApply.x);

        }break;

        default :
            break;

    }


    return 1;
}




