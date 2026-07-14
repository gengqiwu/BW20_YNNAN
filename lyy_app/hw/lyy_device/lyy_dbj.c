/*
 * @author: e.he
 * @Date: 2024-07-09 18:38:17
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-08 11:00:13
 * @Description: 
 */

#include <stdio.h>
#include <string.h>



#include "APP_Param.h"
#include "APP_Pluse.h"
#include "APP_Detect.h"
#include "APP_DrillingMode.h"

#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_WIFI.h"
#include "HAL_Uart.h"
#include "HAL_System.h"

#include "USER_Config.h"

#include "thing_init.h"
#include "thing_iot.h"
#include "thing_mqtt.h"
#include "thing_event.h"
#include "thing_property.h"

#include "lyy_device.h"
#include "lyy_dbj.h"
#include "lyy_tjc.h"


#include "utils.h"


/**
 * @brief
 * 支付申请应答---兑币机
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_dbj_pay_apply_reply(uint8_t *data, uint16_t len)
{

    (void)len;

    if(!data) {

        return ;
    }

    PHW_DBJ_PAY_APPLY_REPLY_DEF pReply = (PHW_DBJ_PAY_APPLY_REPLY_DEF)data;

    g_xlyyHwMark.applyResult = pReply->status;//0x00-失败 0x01-成功

    if(g_xlyyHwMark.ucPaySem)
    {
        xSemaphoreGive(g_xlyyHwMark.pxPayApply);
        g_xlyyHwMark.ucPaySem = 0x00;
    }

    LYY_LOGD("apply_result [DBJ] :%d", g_xlyyHwMark.applyResult);
}


/**
 * @brief
 * 启动---兑币机
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_dbj_pay_start_reply(uint8_t *data, uint16_t len)
{

     (void)len;

    if(!data) {

        return ;
    }

    PHW_DBJ_OUT_COINS_REPLY_DEF pReply = (PHW_DBJ_OUT_COINS_REPLY_DEF)data;

    g_xlyyHwMark.startResult = pReply->result;//0x00-失败 0x01-成功

    if(g_xlyyHwMark.ucPaySem)
    {
        xSemaphoreGive(g_xlyyHwMark.pxPayApply);
        g_xlyyHwMark.ucPaySem = 0x00;
    }

    LYY_LOGD("start_result [DBJ] :%d",g_xlyyHwMark.startResult);

}


/**
 * @brief 
 * 出币结果应答 ---兑币机
 * @param data 
 * @param len 
 */
void device_dbj_reply_out_coins_result(uint8_t *data, uint16_t len)
{

    (void)len;

    if(!data) {

        return ;
    }

    DBJ_OUT_COINS_RESULT_REPLY_DEF reply;
    memcpy(&reply.sud_cmd,data,sizeof(reply));

    reply.result = 0x01;
    device_packet_data_length(0xD1, &reply.sud_cmd, sizeof(DBJ_OUT_COINS_RESULT_REPLY_DEF));

}

/**
 * @brief
 * POS账目应答 ---兑币机    可以和线下账目应答合并取data[0]的数值
 * @param data
 * @param len
 */
void device_dbj_reply_accounts_pos(uint8_t *data, uint16_t len)
{
    (void) len;
    uint8_t sub_cmd = data[0];
    device_packet_data_length(0xD1, &sub_cmd, sizeof(sub_cmd));
}
/**
 * @brief
 * 兑币机-出币结果 POS
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_dbj_accounts_pos(uint8_t *data, uint16_t len)
{
    PDBJ_ACCOUNTS_DEF pAcc = (PDBJ_ACCOUNTS_DEF)data;
    char order[30] = {0};
    utils_lhexToString(order, pAcc->unique, 6);
    event_post_account_dbj_uart("pos_pay", pAcc->money_sum, pAcc->money_inc, pAcc->coins_sum, pAcc->coins_inc, order);
    device_dbj_reply_accounts_pos(data, len);
}

/**
 * @brief
 * 找零应答 ---兑币机    可以和线下账目、POS账目应答合并取data[0]的数值
 * @param data
 * @param len
 */
void device_dbj_reply_accounts_change(uint8_t *data, uint16_t len)
{
    (void)len;
    uint8_t sub_cmd = data[0];
    device_packet_data_length(0xD1, &sub_cmd, sizeof(sub_cmd));
}

/**
 * @brief
 * 兑币机-找零
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_dbj_accounts_change(uint8_t *data, uint16_t len)
{
    PDBJ_CHANGE_DEF pAcc = (PDBJ_CHANGE_DEF)data;
    char order[30] = {0};
    utils_lhexToString(order, pAcc->unique, 6);
    event_post_account_dbj_uart_change("give_change", pAcc->all_money_sum, pAcc->out_money_inc, order);
    device_dbj_reply_accounts_change(data, len);
}

/**
 * @brief
 * 出币结果--兑币机
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_dbj_out_coins_result(uint8_t *data, uint16_t len)
{

    (void)len;

    if(!data) {

        return ;
    }

    #define  OUT_COINS_RESULT(x)   ((x==1) ? "start_normally":"startup_failed") 

    PDBJ_OUT_COINS_RESULT_DEF pReply = (PDBJ_OUT_COINS_RESULT_DEF)data;
    char order[30] ={0};

    utils_lhexToString(order,pReply->unique,6);

    event_post_out_coin_result(OUT_COINS_RESULT(pReply->result), pReply->coins,order,pReply->payAmount);
    device_dbj_reply_out_coins_result(data,len);

}

/**
 * @brief 
 * 线下账目应答 ---兑币机
 * @param data 
 * @param len 
 */
void device_dbj_reply_accounts(uint8_t *data, uint16_t len)
{

    (void)len;
    (void)data;

    uint8_t sub_cmd = 0x06;
    device_packet_data_length(0xD1, &sub_cmd, sizeof(sub_cmd));
}

/**
 * @brief
 * 兑币机-出币结果
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_dbj_accounts_offline(uint8_t *data, uint16_t len)
{

    PDBJ_ACCOUNTS_DEF pAcc = (PDBJ_ACCOUNTS_DEF)data;
    char order[30] ={0};

    utils_lhexToString(order,pAcc->unique,6);
    event_post_account_dbj_uart("offline_pay",pAcc->money_sum,pAcc->money_inc,pAcc->coins_sum,pAcc->coins_inc,order);
    device_dbj_reply_accounts(data,len);

}

/**
 * @brief
 * dbj 账目应答
 */
void device_dbj_reply_accounts_update(void)
{
    DEVICE_PACK_DEF tDevPack;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tDevPack.cmd = 0x14;
    tDevPack.len = 3;
    device_buildPacket(&tDevPack);

}

/**
 * @brief
 * 主板上报的兑币机账目信息处理
 * @param package
 * @param len
 */
void device_dbj_accounts_update(uint8_t *package, uint16_t len)
{

    (void)len;

    if(!package) {

        return ;
    }

    DEVICE_ACC_DBJ_DEF *devAcc = (DEVICE_ACC_DBJ_DEF *)(package + 4);

    uint32_t acc_inc = 0;
    static uint8_t mark = 0;
    static uint8_t fault = 0;

    device_dbj_reply_accounts_update();      //先应答

    if(devAcc->mark == mark) //重发数据
        return;

    mark =  devAcc->mark;

    if(devAcc->fault != fault) //故障状态变化需要上传
    {
        fault = devAcc->fault;
        ctlb_up_raw_board_error(fault);
        lyy_tjc_page_fault_set(fault);
    }    

    if (devAcc->out_coin != 0 || devAcc->free_coin != 0) // 有出币增量
    {
        acc_inc = devAcc->out_coin + devAcc->free_coin;
        APP_vDbjCoinOut(acc_inc);
    }

    //投币
    if (devAcc->note > 0)
    {
        acc_inc = devAcc->note;
        APP_vDbjCoinIn(acc_inc);
    }

}
/**
 * @brief
 * 出币结果上报应答
 * @param trade
 * @param result
 */
void device_dbj_reply_out_coins_result_03(uint8_t *pack, uint16_t len)
{

    (void)len;

    if(!pack) {

        return ;
    }

    DEVICE_PACK_DEF tDevPack;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tDevPack.cmd = 0xd1;
    tDevPack.len = 11;

    memcpy(tDevPack.data, pack, 7);

    tDevPack.data[0] = 0x03;
    tDevPack.data[7] = 0x01;

    device_buildPacket(&tDevPack);

}
/**
 * @brief 
 * 03 指令--出币结果
 * @param pack 
 * @param len 
 */
void device_dbj_out_coins_result_03(uint8_t *pack, uint16_t len)
{

    #define  OUT_COINS_RESULT(x)   ((x==1) ? "start_normally":"startup_failed") 
    PDBJ_OUT_COINS_RESULT03_DEF pReply = (PDBJ_OUT_COINS_RESULT03_DEF)pack;

    char order[30] ={0};
    device_dbj_reply_out_coins_result_03(pack,len);   
    utils_lhexToString(order,pReply->unique,6);

    event_post_out_coin_result(OUT_COINS_RESULT(pReply->result), pReply->coins,order,pReply->payAmount); 

}

/**
 * @brief
 * 出币应答-20
 */
void device_dbj_reply_coins_cmd_20(void)
{
    DEVICE_PACK_DEF tDevPack;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tDevPack.cmd = 0x20;
    tDevPack.len = 3;
    device_buildPacket(&tDevPack);

}

/**
 * @brief 
 * 
 * @param pack 
 * @param len 
 */
void device_dbj_out_coins_cmd_20(uint8_t *pack,uint16_t len)
{

    (void)len;

    if(!pack) {

        return ;
    }

    DBJ_EXPORT_COIN_20_T *coins_out = (DBJ_EXPORT_COIN_20_T *)(pack+4);
    device_dbj_reply_coins_cmd_20();

    lyy_tjc_dbj_coins_set(coins_out->total,coins_out->inc);

}
/**
 * @brief
 * 出币应答-D120
 */
void device_dbj_reply_coins_cmd_D120(void)
{
    DEVICE_PACK_DEF tDevPack;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tDevPack.cmd = 0xd1;
    tDevPack.data[0] = 0x20;
    tDevPack.len = 4;
    device_buildPacket(&tDevPack);
}
/**
 * @brief 
 * 
 * @param pack 
 * @param len 
 */
void device_dbj_out_coins_cmd_d120(uint8_t *pack,uint16_t len)
{
    (void)len;

    if(!pack) {

        return ;
    }

    DBJ_EXPORT_COIN_D120_T *coins_out = (DBJ_EXPORT_COIN_D120_T *)(pack+1);
    device_dbj_reply_coins_cmd_D120();

    lyy_tjc_dbj_coins_set(coins_out->total,coins_out->inc);

}


/**
 * @brief 
 * 支付申请---兑币机
 * @param unqueid 
 * @return lyy_int8_t 
 */
int device_dbj_pay_apply(void)
{

    HW_DBJ_PAY_APPLY_DEF payApply;

    LYY_LOGD("device_dbj_pay_apply");   
    vRandomGet((char *)payApply.unique, sizeof(payApply.unique));

    payApply.sud_cmd = 0x01;

    device_packet_data_length(0xD1, (uint8_t *)&payApply, sizeof(HW_DBJ_PAY_APPLY_DEF));

    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 3000); //等待主板启动应答

}

/**  
 * @brief 
 * 支付启动
 * @param unqueid 
 * @param payMount 
 * @param coins 
 * @return lyy_int8_t 
 */
int device_dbj_pay_start(uint16_t payMount, uint16_t coins)
{

    HW_DBJ_OUT_COINS_DEF outCoins;

    vRandomGet((char *)outCoins.unique, sizeof(outCoins.unique));
    outCoins.sud_cmd = 0x02;
    outCoins.payAmount = payMount;
    outCoins.coins = coins;
    
    device_packet_data_length(0xD1, (uint8_t *)&outCoins, sizeof(HW_DBJ_OUT_COINS_DEF));

    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 5 * 1000); //等待主板启动应答

}

/**
 * @brief 
 * 海外兑币机指令解析
 * @param pBuf 数据
 * @param len 数据长度
 */
void device_dbj_packet_parse(uint8_t subCmd, uint8_t *data, uint16_t len)
{
    if(data == NULL)
        return ;
    switch (subCmd)
    {
        case 0x01:  // 支付申请应答
            device_dbj_pay_apply_reply(data, len);
            break;   
        case 0x02:  //出币应答
            device_dbj_pay_start_reply(data, len);
            break;
        case 0x03:  //出币结果应答
            device_dbj_reply_out_coins_result_03(data, len);
            break;
        case 0x05:  //上传出币结果
            device_dbj_out_coins_result(data, len);
            break;
        case 0x06:  //上传线下账目
            device_dbj_accounts_offline(data, len);
            break;
        case 0x07: // 上传POS账目
            device_dbj_accounts_pos(data, len);
            break;
        case 0x08: // 上传找零账目
            device_dbj_accounts_change(data, len);
            break;
        case 0x20:  //出币上报
            device_dbj_out_coins_cmd_d120(data, len);
            break;
        default: break;
    }
}



