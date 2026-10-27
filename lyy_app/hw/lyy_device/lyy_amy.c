/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:51
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 16:46:18
 * @Description: 
 */
#include <stdio.h>
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
#include "lyy_amy.h"
#include "lyy_tjc.h"


#include "utils.h"

AMY_STATE_t g_tAMYState;

extern void device_amy_update_online(uint32_t time);

/**
 * @brief 
 * 故障 显示标志
 * @param data 
 * @param len 
 */
uint8_t device_get_amy_err_Flag(void)
{
    LYY_LOGD("device_get_amy_err_Flag [%d]",g_tAMYState.ErrShowFlag);
    return  g_tAMYState.ErrShowFlag;
}
/**
 * @brief 
 * 故障 显示错误码
 * @param data 
 * @param len 
 */
uint8_t device_get_amy_err_code(void)
{
    LYY_LOGD("device_get_amy_err_code [%d]",g_tAMYState.ErrShowCode);
    return g_tAMYState.ErrShowCode;
}
/**
 * @brief 
 * 故障 显示错误码
 * @param data 
 * @param len 
 */
void device_set_amy_err_power(uint8_t power)
{

    (void)power;

    g_tAMYState.ErrPowerOnFlag = 1;
    LYY_LOGD("device_set_amy_err_power");
}
/**
 * @brief 
 * 开机是否有故障显示
 */
void device_get_amy_err_powerOn(void)
{
    DEV_FAULT *fault = get_dev_fault_para();
    uint8_t err = device_get_amy_err_code();
    device_set_amy_err_power(1); //已启动
    if(!device_get_amy_err_Flag()) return ;

    for(int i = 0;i <= fault->num;i++)
    {
        // 设定的故障
        if(err == fault->fault_t[i].errCode)
        {
            LYY_LOGD("err [amy_code] : [%d][%d] ", err,fault->fault_t[i].errCode);
            if(fault->fault_t[i].errFlag)
            {
                // 锁定显示故障信息
                lyy_tjc_set_status(TJC_SHOW_WARING_TIP);
                lyy_tjc_page_obj_amy_waring(fault->fault_t[i].errMsg);
            }
        }
    }
}

/**
 * @brief 
 * 主板状态上报
 * 故障
 * @param data 
 * @param len 
 */
void device_amy_err_deal(uint8_t err)
{
    // pVoice_Param->Errbutton = 1;
    // if(!pVoice_Param->Errbutton) return;
    
    DEV_FAULT *fault = get_dev_fault_para();
    LYY_LOGD("err [amy] : [%d] ", fault->num);
    for(int i = 0;i <= fault->num;i++)
    {
        // 设定的故障
        if(err == fault->fault_t[i].errCode)
        {
            LYY_LOGD("err [amy_code] : [%d][%d] ", err,fault->fault_t[i].errCode);
            if(fault->fault_t[i].errFlag)     
            { 
                if(g_tAMYState.ErrPowerOnFlag) // 屏幕已经初始化完成
                {
                    // 锁定显示故障信息
                    lyy_tjc_set_status(TJC_SHOW_WARING_TIP);
                    lyy_tjc_page_obj_amy_waring(fault->fault_t[i].errMsg);
                }
                g_tAMYState.ErrShowFlag = 1;
                g_tAMYState.ErrShowCode = err;
                return;
            }
        }
    }

    if(g_tAMYState.ErrShowFlag)
    {
        // 解除故障信息显示
        g_tAMYState.ErrShowFlag = 0;
        g_tAMYState.ErrShowCode = 0;

        // 恢复时间为0
        device_amy_update_online(0);
        lyy_tjc_set_status(TJC_SHOW_PACKAGES);

    }
}

/**
 * @brief 
 * 支付申请 -- 按摩椅
 * @return int 
 */
int device_amy_pay_apply(void)
{
    LYY_LOGD("device_amy_pay_apply"); 

    static uint8_t amy_id   = 0x27;

    HW_AMY_PAY_APPLY_DEF payApply;
    payApply.sud_cmd        = 0x14;
    payApply.add            = amy_id++;

    vRandomGet(payApply.unique, sizeof(payApply.unique));
    
    device_packet_data_length(0x37, (uint8_t *)&payApply, sizeof(HW_AMY_PAY_APPLY_DEF));
    
    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 5000); 

}

/**
 * @brief 
 * 支付启动--按摩椅
 * @param time 
 * @return int 
 */
int device_amy_pay_start(uint16_t time)
{
    
    HW_AMY_START_DEF amyStart;

    vRandomGet(amyStart.unique, sizeof(amyStart.unique));

    static uint8_t amy_id   = 0x27;
    amyStart.sud_cmd        = 0x01;
    amyStart.add            = amy_id++;

    amyStart.time           = time;//
    amyStart.mode           = 0x0001;//0x0001 男士 0x0002 女士 0x0003 老人
    
    LYY_LOGD("amy [time] : %d", amyStart.time * 60);
    device_packet_data_length(0x37, (uint8_t *)&amyStart, sizeof(HW_AMY_START_DEF));

    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 5000); //等待主板启动应答

}

/**
 * @brief
 * 支付申请应答--按摩椅
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_amy_reply_pay_apply(uint8_t *data, uint16_t len)
{

    (void)len;

    PHW_AMY_PAY_APPLY_REP_DEF pReply    = (PHW_AMY_PAY_APPLY_REP_DEF)data;
    g_xlyyHwMark.startResult            = pReply->status;

    if(g_xlyyHwMark.ucPaySem)
    {
        xSemaphoreGive(g_xlyyHwMark.pxPayApply);
        g_xlyyHwMark.ucPaySem = 0x00;
    }

    LYY_LOGD("apply[amy] :%d", g_xlyyHwMark.startResult);

}

/**
 * @brief
 * 支付启动应答---按摩椅
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_amy_reply_pay_start(uint8_t *data, uint16_t len)
{

    (void)len;

    PHW_AMY_PAY_APPLY_REP_DEF pReply    = (PHW_AMY_PAY_APPLY_REP_DEF)data;
    g_xlyyHwMark.startResult            =  pReply->status;

    if(g_xlyyHwMark.ucPaySem)
    {
        xSemaphoreGive(g_xlyyHwMark.pxPayApply);
        g_xlyyHwMark.ucPaySem = 0x00;
    }

    LYY_LOGD("start[amy] :%d",g_xlyyHwMark.startResult);

}

/**
 * @brief
 * 账目
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_amy_update_acc(uint8_t *data, uint16_t len)
{

    (void)len;

    static uint8_t add      = 0xA9;
    AMY_ACC_REPLY_DEF reply = {0x0a,0x01,0x01};
    PAMY_ACC_DEF acc_amy    = (PAMY_ACC_DEF)data;
    reply.sud_cmd           = 0x0a;
    reply.add               = acc_amy->add;
    reply.result            = 1; //成功

    device_packet_data_length(0x37, (uint8_t *)&reply, sizeof(reply));

    if(acc_amy->add != add) //需要上报
    {
        add = acc_amy->add;
        //event_post_account_amy_uart("offline_pay",0,0,acc_amy->total,acc_amy->inc, NULL);

        offline_accounts_save(acc_amy->inc, acc_amy->total, MSG_TYPE_AMOUNT, PAY_METHOD_OFFLINE);

        LYY_LOGD("acc[amy] total: %d ,inc: %d",acc_amy->total,acc_amy->inc);
    }

}
/**
 * @brief
 * 账目
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_amy_update_acc_V2(uint8_t *data, uint16_t len)
{

    (void)len;

    static uint8_t add = 0xA9;
    AMY_ACC_REPLY_DEF reply = {0x0C,0x01,0x01};
    PAMY_ACC_DEF_V2 acc_amy = (PAMY_ACC_DEF_V2)data;
    reply.sud_cmd = 0x0C;
    reply.add = acc_amy->add;
    reply.result = 1; //成功
    device_packet_data_length(0x37, (uint8_t *)&reply, sizeof(reply));
    if(acc_amy->add != add) //需要上报
    {
        add = acc_amy->add;
        //event_post_account_amy_uart("offline_pay",acc_amy->coin_total,acc_amy->coin_inc,acc_amy->cash_total,acc_amy->cash_inc, NULL);

        offline_accounts_save(acc_amy->cash_inc, acc_amy->cash_total, MSG_TYPE_AMOUNT, PAY_METHOD_OFFLINE);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        offline_accounts_save(acc_amy->coin_inc, acc_amy->coin_total, MSG_TYPE_COIN_IN, PAY_METHOD_OFFLINE);

        LYY_LOGD("acc[amy] coin total: %d ,inc: %d",acc_amy->coin_total,acc_amy->coin_inc);
        LYY_LOGD("acc[amy] cash total: %d ,inc: %d",acc_amy->cash_total,acc_amy->cash_inc);
    }
}
/**
 * @brief 
 * 主板状态上报
 * 故障
 * @param data 
 * @param len 
 */
void device_amy_update_status(uint8_t *data, uint16_t len)
{
    (void)len;

    static uint8_t add  = 0xA9;
    uint8_t reply[3]    = {0x09,0x02,0x01};
    reply[0]            = 0x09;
    reply[1]            = data[1];
    reply[2]            = 1; //成功

    device_packet_data_length(0x37, reply, sizeof(reply));

    if(data[1] != add) //需要上报
    {
        add = data[1];
        ctlb_up_raw_board_error(data[2]); //上报故障
        LYY_LOGD("status[amy] : %d ,add: %d",data[2],add);

        //增加判断致命性故障进入故障页面，其他则恢复到套餐页面。
        device_amy_err_deal(data[2]);
    }

}
/**
 * @brief 
 * 
 */
void device_amy_update_online(uint32_t time)
{

    amy_time_t.sys_tick_start   = HAL_ulGetSysTick()/1000;
    tjc_infor_t.amy_time.total  = time;
    amy_time_t.sys_tick_value   = amy_time_t.sys_tick_start;

}
/**
 * @brief 
 * 主板状态上报
 * 不应答 
 * @param data 
 * @param len 
 */
void device_amy_update_status_V2(uint8_t *data, uint16_t len)
{

    (void)len;

    static uint8_t fault = 0;
    // 未开机完成
    if(g_tAMYState.ErrPowerOnFlag == 0) return;
    AMY_STATUS_DEF_V2 *amy_status =(AMY_STATUS_DEF_V2 *)data;

    // 故障处理
    if(amy_status->fault != fault) 
    {
        fault = amy_status->fault;
        ctlb_up_raw_board_error(data[2]); //上报故障
        //增加判断致命性故障进入故障页面，其他则恢复到套餐页面。
        device_amy_err_deal(data[2]);
        return ;
    }else{
        if(g_tAMYState.ErrShowFlag) 
        {
            // 有线上
            g_tAMYState.OffStatusFlag = 0;
            return;
        }
    }
    
    // 按摩椅状态处理
    if(amy_status->status == 0x00 ||  amy_status->time == 0) //空闲
    {
        // 空闲标志 直接退出
        if(g_tAMYState.OffStatusFlag == 0) return;
        // 回到套餐页面
        g_tAMYState.OffStatusFlag = 0;

        lyy_tjc_set_status(TJC_SHOW_PACKAGES);

        g_tAMYState.work_state = 0;
        // // 播放结束语音
        // amy_music_play(VOICE_CMD_SET_VOICE,VOICE_FILE_FINISH);
        // 同步线上更新情况
        device_amy_update_online(0);

    }else if(amy_status->status == 0x01) // 启动
    { 
        if(g_tAMYState.OffStatusFlag == 0)
        {

            // 进入倒计时页面
            lyy_tjc_set_status(TJC_SHOW_AMY_TIME_WORK_V2);
            lyy_tjc_page_amy_working_V2(g_tAMYState.OffStatusFlag,amy_status->time);

            // 同步线上更新情况
            device_amy_update_online(amy_status->time);
            g_tAMYState.OffStatusFlag = 1;

            // 播放开始语音
            // if(g_tAMYState.work_state == false)
            //     amy_music_play(VOICE_CMD_SET_VOICE,VOICE_FILE_START); 
            g_tAMYState.work_state = 1;

        }else
        {
            // 同步线上更新情况
            device_amy_update_online(amy_status->time);
            // 或者进行时间刷新
            g_tAMYState.work_state = 1;

            lyy_tjc_page_amy_working_V2(g_tAMYState.OffStatusFlag,amy_status->time);

        }
    }
}

/**
 * @brief
 * 按摩椅
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_amy_packet_parse(uint8_t subCmd, uint8_t *data, uint16_t len)
{

    if(data == NULL)
        return ;

    LYY_LOGD("device_amy_packet_parse subCmd:%02X", subCmd);

    switch (subCmd)
    {
        case 0x14:  // 支付申请应答
            device_amy_reply_pay_apply(data, len);
            break;
        case 0x01:  //启动应答
            device_amy_reply_pay_start(data, len);
            break;
        case 0x09:  //状态上报
            device_amy_update_status(data, len);
            break;
        case 0x0A:  //账目上报
            device_amy_update_acc(data, len);
            break;
        case 0x0B:  //剩余时间和状态上报
            device_amy_update_status_V2(data, len);
            break;
        case 0x0C:  //账目上报(投币+现金)
            device_amy_update_acc_V2(data, len);
            
            break;
        default: break; 
    }

}