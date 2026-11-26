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
#include "lyy_para.h"
#include "lyy_start.h"
#include "lyy_device.h"
#include "lyy_xyj.h"
#include "lyy_hgj.h"
#include "lyy_cwhgj.h"
#include "lyy_cwxyj.h"
#include "lyy_cwxhj.h"
#include "lyy_mshgj.h"
#include "lyy_tclxyj.h"
#include "lyy_dfssj.h"
#include "lyy_xcj.h"

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

        case DEVICE_TYPE_PULSES:
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
        case DEVICE_TYPE_UART_LJXYJ: // 乔禾洗衣机
        case DEVICE_TYPE_UART_QHXYJ: // 乔禾洗衣机
        {
            ret = (device_qhxyj_pay_apply() > 0) ? apply_start_t.apply_result : PAY_START_RESULT_TIMEOUT; //
            /*是否应答超时--》有应答，是否设备正常*/
            thing_service_reply_pay_apply_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_LJHGJ: // 乔禾烘干机
        case DEVICE_TYPE_UART_QHHGJ: // 乔禾烘干机
        {
            ret = (device_qhhgj_pay_apply() > 0) ? apply_start_t.apply_result : PAY_START_RESULT_TIMEOUT; //
            /*是否应答超时--》有应答，是否设备正常*/
            thing_service_reply_pay_apply_qhhgj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_SKYWORTH_GTXYJ: // 创维滚筒洗衣机
        {
            ret = (skyworth_qhxyj_pay_apply() > 0) ? apply_start_t.apply_result : PAY_START_RESULT_TIMEOUT; //
            /*是否应答超时--》有应答，是否设备正常*/
            thing_service_reply_pay_apply_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_SKYWORTH_GTHGJ: //
        {
            ret = (skyworth_qhhgj_pay_apply() > 0) ? apply_start_t.apply_result : PAY_START_RESULT_TIMEOUT; //
            /*是否应答超时--》有应答，是否设备正常*/
            thing_service_reply_pay_apply_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_SKYWORTH_GTXHJ: //
        {
            ret = (skyworth_qhxhj_pay_apply() > 0) ? apply_start_t.apply_result : PAY_START_RESULT_TIMEOUT; //
            /*是否应答超时--》有应答，是否设备正常*/
            thing_service_reply_pay_apply_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_MSHGJ: // 
        {
            LYY_LOGD("mshgj_pay_apply");
            ret = (mshgj_pay_apply() > 0) ? apply_start_t.apply_result : PAY_START_RESULT_TIMEOUT; //
            /*是否应答超时--》有应答，是否设备正常*/
            thing_service_reply_pay_apply_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_TCLXYJ: // TCL洗衣机
        {
            LYY_LOGD("device_tclxyj_pay_apply");
            ret = (device_tclxyj_pay_apply() > 0) ? apply_start_t.apply_result : PAY_START_RESULT_TIMEOUT; //
            /*是否应答超时--》有应答，是否设备正常*/
            thing_service_reply_pay_apply_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_DFSSJ: // 售水机
        {
            ret = (device_dfssj_pay_apply(pxPayApply->x + 1) > 0) ? 1 : 0;
            LYY_LOGD("ret:%d",ret);
            thing_service_reply_pay_apply_ndj(pxPayApply->x + 1, pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);  
        }
        break;
        case DEVICE_TYPE_UART_THXCJ: // 洗车机
        {
            ret = (xcj_pay_apply() > 0) ? 1 : 0; //
            thing_service_reply_pay_apply_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
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
        case DEVICE_TYPE_UART_LJXYJ: // 蓝鲸洗衣机
        case DEVICE_TYPE_UART_QHXYJ: // 乔禾洗衣机
        {
            apply_start_t.start_result = 0x00;
            start_ret = device_qhxyj_pay_start(pxPayApply->number);
            LYY_LOGD("start_ret[qhxyj]:%d", start_ret);
            ret = start_ret > 0 ? apply_start_t.start_result : PAY_START_RESULT_TIMEOUT; //
            thing_service_reply_pay_start_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_LJHGJ: // 蓝鲸烘干机
        case DEVICE_TYPE_UART_QHHGJ: // 乔禾烘干机
        {
            apply_start_t.start_result = 0x00;
            start_ret = device_qhhgj_pay_start(pxPayApply->number);
            LYY_LOGD("start_ret[qhhgj]:%d", start_ret);
            ret = start_ret > 0 ? apply_start_t.start_result : PAY_START_RESULT_TIMEOUT; //
            thing_service_reply_pay_start_qhhgj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_SKYWORTH_GTXYJ: // 创维滚筒洗衣机
        {
            start_ret = skyworth_xyj_pay_start(pxPayApply->number);
            LYY_LOGD("start_ret[cwxyj]:%d", start_ret);
            ret = start_ret > 0 ? apply_start_t.start_result : PAY_START_RESULT_TIMEOUT; //
            thing_service_reply_pay_start_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);

            if (apply_start_t.start_result == 1)
            { // 洗衣机启动成功，启动加液机
                CYJYJ_start(15, 15);
            }
        }
        break;
        case DEVICE_TYPE_UART_SKYWORTH_GTHGJ: //
        {
            start_ret = skyworth_hgj_pay_start(pxPayApply->number);
            LYY_LOGD("start_ret[cwxyj]:%d", start_ret);
            ret = start_ret > 0 ? apply_start_t.start_result : PAY_START_RESULT_TIMEOUT; //
            thing_service_reply_pay_start_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_SKYWORTH_GTXHJ: //
        {
            start_ret = skyworth_xhj_pay_start(pxPayApply->number);
            LYY_LOGD("start_ret[cwxhj]:%d", start_ret);
            ret = start_ret > 0 ? apply_start_t.start_result : PAY_START_RESULT_TIMEOUT; //
            thing_service_reply_pay_start_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_MSHGJ: //麦穗烘干机
        {
            start_ret = mshgj_pay_start(pxPayApply->number);
            LYY_LOGD("start_ret[cwxyj]:%d", start_ret);
            ret = start_ret > 0 ? apply_start_t.start_result : PAY_START_RESULT_TIMEOUT; //
            thing_service_reply_pay_start_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_TCLXYJ: //
        {
            start_ret = device_tclxyj_pay_start(pxPayApply->number);
            LYY_LOGD("start_ret[cwxyj]:%d", start_ret);
            ret = start_ret > 0 ? apply_start_t.start_result : PAY_START_RESULT_TIMEOUT; //
            thing_service_reply_pay_start_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_DFSSJ:
        {
            PDFSSJ_UPDATE_EVENT_DEF infor = dfssj_get_update_event_infor();
            g_xlyyHwMark.startResult = 0;
            memset(infor->start[pxPayApply->x].order, 0, sizeof(infor->start[pxPayApply->x].order));
            strcpy(infor->start[pxPayApply->x].order, pxPayApply->uniqueid); // 缓存订单号
            infor->start[pxPayApply->x].flag = false;                        // 清零上报标识
            g_xlyyHwMark.ucPaySem = 0x01;
            result = (deivce_dfssj_pay_start(pxPayApply->x + 1, pxPayApply->number) > 0) ? g_xlyyHwMark.startResult : PAY_START_RESULT_TIMEOUT;
            g_xlyyHwMark.ucPaySem = 0x00;

            LYY_LOGD("result[ndj]:%d", result);
            thing_service_reply_pay_start_dfssj(pxPayApply->x + 1, pxPayApply->uniqueid, result, pxPayApply->pay_apply_id);
        }
        break;
        case DEVICE_TYPE_UART_THXCJ:
        {
            apply_start_t.start_result = 0x00;
            start_ret = xcj_pay_start(10,pxPayApply->number);
            LYY_LOGD("start_ret[cwxyj]:%d", start_ret);
            ret = start_ret > 0 ? apply_start_t.start_result : PAY_START_RESULT_TIMEOUT; //
            thing_service_reply_pay_start_qhxyj(pxPayApply->uniqueid, ret, pxPayApply->pay_apply_id);
        }
        break;
        default:break;
    }          
}


/* APP_vEventSendBuff has no return status. Require an acknowledgement from
 * the existing start task, and cancel the ticket if it never gets there
 * (full ring, allocation failure, or stalled worker). No late GPIO output. */
typedef struct {
    HW_PAY_APPLY xPay;
    uint32_t ulTicket;
} PULSES_START_EVENT_T;

enum {
    PULSES_START_FREE,
    PULSES_START_WAITING,
    PULSES_START_REJECTED,
    PULSES_START_ACCEPTED,
};

static uint8_t g_ucPulsesStartState;
static uint32_t g_ulPulsesStartTicket;

/**
 * @brief 启动任务处理双货道请求，仅接受仍在等待确认的票据。
 * @param pucData 队列中的启动事件数据。
 * @param ulLen 事件数据字节数。
 */
static void lyy_start_pay_start_pulses(uint8_t *pucData, uint32_t ulLen)
{
    if (pucData == NULL || ulLen != sizeof(PULSES_START_EVENT_T)) {
        return;
    }
    const PULSES_START_EVENT_T *pxEvent = (const PULSES_START_EVENT_T *)pucData;
    const bool bDualLane = (get_dev_type_code() == DEVICE_TYPE_PULSES);

    taskENTER_CRITICAL();
    if (g_ucPulsesStartState == PULSES_START_WAITING &&
        g_ulPulsesStartTicket == pxEvent->ulTicket) {
        const bool bAccepted = bDualLane && pxEvent->xPay.y == 0 &&
                               APP_bTryLanePulseStart(pxEvent->xPay.x, pxEvent->xPay.number);
        g_ucPulsesStartState = bAccepted ? PULSES_START_ACCEPTED : PULSES_START_REJECTED;
    }
    taskEXIT_CRITICAL();
}

/**
 * @brief 提交双货道线上启动，并等待脉冲队列接收确认。
 * @param num 订单要求输出的脉冲数。
 * @param order 云端订单号。
 * @param x 零基输出通道；0 为货道 1，1 为货道 2。
 * @param y 纵坐标，必须为 0。
 * @param id 云端请求标识，用于对应服务应答。
 * @return 接收成功为 1，无效、忙或超时为 0；不代表出货完成。
 * @note 等待最多约 2 秒；超时后迟到事件不会再驱动输出。
 */
int lyy_start_submit_pulses(uint16_t num, const char *order, uint16_t x, uint16_t y, const char *id)
{
    PULSES_START_EVENT_T xEvent = {0};
    if (num == 0 || x >= 2 || y != 0 ||
        order == NULL || order[0] == '\0' || strlen(order) >= sizeof(xEvent.xPay.uniqueid) ||
        id == NULL || id[0] == '\0' || strlen(id) >= sizeof(xEvent.xPay.pay_apply_id)) {
        return 0;
    }
    strcpy(xEvent.xPay.uniqueid, order);
    strcpy(xEvent.xPay.pay_apply_id, id);
    xEvent.xPay.number = num;
    xEvent.xPay.x = x;
    xEvent.xPay.y = y;

    taskENTER_CRITICAL();
    if (g_ucPulsesStartState != PULSES_START_FREE) {
        taskEXIT_CRITICAL();
        return 0;
    }
    xEvent.ulTicket = ++g_ulPulsesStartTicket;
    g_ucPulsesStartState = PULSES_START_WAITING;
    taskEXIT_CRITICAL();

    /* The task services one start event per 50 ms (ring capacity: 30). */
    const TickType_t xStart = xTaskGetTickCount();
    APP_vEventSendBuff(eEventStart, lyy_start_pay_start_pulses,
                       (uint8_t *)&xEvent, sizeof(xEvent));
    for (;;) {
        taskENTER_CRITICAL();
        if (g_ucPulsesStartState != PULSES_START_WAITING ||
            (TickType_t)(xTaskGetTickCount() - xStart) >= pdMS_TO_TICKS(2000U)) {
            int accepted = (g_ucPulsesStartState == PULSES_START_ACCEPTED);
            g_ucPulsesStartState = PULSES_START_FREE;
            taskEXIT_CRITICAL();
            return accepted;
        }
        taskEXIT_CRITICAL();
        vTaskDelay(1);
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




