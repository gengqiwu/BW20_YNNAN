/*
 * @author: e.he
 * @Date: 2024-07-10 16:29:48
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 15:54:13
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

#include "USER_Config.h"

#include "thing_init.h"
#include "thing_iot.h"
#include "thing_mqtt.h"
#include "thing_event.h"
#include "thing_property.h"

#include "lyy_device.h"
#include "lyy_wwj.h"
#include "lyy_tjc.h"


#include "utils.h"


/**
 * @brief 
 * 支付申请
 * @param pay_id 
 * @param sub_id 
 * @param id 
 * @return uint8_t 
 * > 0 成功
 * =0 失败
 */
int device_wwj_pay_apply(void)
{
 
    device_qureyConect(TYPE_QUREY_PAY_APPLY);

    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 3000); //等待主板启动应答

}


/**
 * @brief
 * wwj账目应答
 */
void device_wwj_reply_accounts_update(void)
{

    static uint16_t test_parse_acc_cnt = 0;

    char ret;

    DEVICE_PACK_DEF tDevPack;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tDevPack.cmd = 0x13;
    tDevPack.len = 3;
    ret = device_buildPacket(&tDevPack);

    LYY_LOGD("device_wwj_reply_accounts_update ret :%d,%d",ret, test_parse_acc_cnt++);

}


/**
 * @brief
 * wwj 账目信息
 * @param package
 * @param len
 */
void device_wwj_accounts_update(uint8_t *package, uint16_t len)
{

    (void)len;

    if(!package) {

        return ;
    }

    DEVICE_ACC_WWJ_DEF *devAcc_wwj = (DEVICE_ACC_WWJ_DEF *)(package + 4);
    uint32_t acc_inc = 0;
    static uint8_t mark = 0;
    static uint8_t fault = 0;
    
    device_wwj_reply_accounts_update();          //先应答

    if(devAcc_wwj->mark == mark) //重发数据
        return ;

    mark = devAcc_wwj->mark;
    if (devAcc_wwj->fault != fault) //故障状态变化需要上传
    {
        fault = devAcc_wwj->fault;
        ctlb_up_raw_board_error(devAcc_wwj->fault);
        lyy_tjc_page_fault_set(fault);

    }

    if (devAcc_wwj->coin != 0 || devAcc_wwj->note != 0) // 有投币增量
    {
        acc_inc = devAcc_wwj->coin + devAcc_wwj->note;
        LYY_LOGD("COIN IN : [%d]", acc_inc);
        APP_vWwjCoinIn(acc_inc);
    }

    //出奖账目
    if(devAcc_wwj->gift > 0)
    {
        acc_inc = devAcc_wwj->gift;
        LYY_LOGD("gift IN : [%d]", acc_inc);
        APP_vWwjGiftOut(acc_inc);
    }

}


/**
 * @brief
 * 上分标识自增
 * @return uint8_t
 */
uint8_t device_getCoinMark(void)
{
    tdevCom.netCoin_mark = (tdevCom.netCoin_mark == 0) ? INIT_NET_COIN_MARK : tdevCom.netCoin_mark;
    tdevCom.netCoin_mark++;
    return tdevCom.netCoin_mark;
}


/**
 * @brief
 * 发送上分指令
 * @param coin
 * @param money
 */
void device_netCoin(uint16_t coin, uint16_t money)
{

    DEVICE_PACK_DEF tDevPack;

    memset(&tDevPack, 0, sizeof(tDevPack));
    tDevPack.cmd = 0x03;
    tDevPack.len = 0x08;
    tDevPack.data[0] = device_getCoinMark();

    data_little16Toarray(coin, tDevPack.data + 1);
    data_little16Toarray(money, tDevPack.data + 3);
    device_buildPacket(&tDevPack);

}


/**
 * @brief
 * 主板上分
 * 阻塞等待上分应答 超时时间为5秒
 * @param coin
 * @param money
 * @return uint8_t
 * TRUE :上分成功
 * FALSE :应答超时
 */
int device_wwj_pay_start(uint16_t coin, uint16_t money)
{

    device_netCoin(coin, money);
    LYY_LOGD("device_wwj_pay_start");

    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 5000); //等待主板启动应答

}



