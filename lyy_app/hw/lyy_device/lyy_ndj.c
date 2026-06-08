/*
 * @author: e.he
 * @Date: 2024-07-02 16:12:11
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-08 19:41:16
 * @Description: 
 */

#include <stdio.h>
#include <string.h>



#include "cJSON.h"

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
#include "thing_service.h"
#include "thing_property.h"

#include "lyy_device.h"
#include "lyy_ndj.h"
#include "lyy_tjc.h"
#include "lyy_para.h"

#include "utils.h"

NDJ_INFOR               ndj_infor_t;
NDJ_UPDATE_EVENT_DEF    ndj_update_event_t ={0};



uint8_t channel_sum ;

uint8_t get_channel_sum(void)
{
    return channel_sum;
}
/**
 * @brief Set the channel sum object
 * 
 * @param sum 
 */
void set_channel_sum(uint8_t sum)
{
    channel_sum = sum;
}
/**
 * @brief 
 * 支付显示相关指令
 * @param id 
 * @param pdata 
 * @param len 
 */
void device_ndj_dzy_paytjc_send(uint8_t id,uint8_t sub_cmd,uint8_t *pdata,uint16_t len)
{
    uint16_t data_len;
    uint16_t farme_len;
    DEVICE_PACK_DEF packet = {0};
    uint8_t *tp = &packet.head;
    #define NDJ_CMD_PAY_TJC     0X07
    memset(&packet,0,sizeof(DEVICE_PACK_DEF));
    packet.cmd = NDJ_CMD_PAY_TJC;
    data_len = len+3+2;
    LYY_LOGD("data_len :%d",data_len);
    packet.len = data_len&0x00ff;
    packet.index = data_len>>8;
    packet.data[0] = 0x01;
    packet.data[1] = sub_cmd;
    packet.data[2] = id;
    memcpy(packet.data+3,pdata,len); //复制数据
    
    packet.head  = 0xAA;
    packet.end   = 0xDD;
    farme_len = data_len + 4;
    packet.check = check_xor_memory(tp+1, farme_len-3);
    tp[farme_len - 2] = packet.check;
    tp[farme_len - 1] = packet.end;
    LYY_LOGD("device_ndj_dzy_paytjc_send:%d",farme_len);
    // LYY_LOGD_hex(tp,farme_len);
        HAL_vUartSend(HAL_UART_MAIN_BOARD, tp, farme_len, 500);
}
/**
 * @brief 
 * 获取渠道信息应答
 * @param id 
 * @param number 
 */
void ndj_dzy_get_pay_channel_ack(uint8_t id,uint8_t number)
{
    NDJ_CHANNEL_INFOR infor ={0};
    PAY_PARAM_DEF *para = get_pay_param();
    uint8_t num = number -1; //从1开始 
    infor.sum = get_channel_sum();
    infor.number = number;
    // lyy_printf("channel_sum :%d,channel_number :%d",infor.sum,infor.number);
    // lyy_printf("code : %s",para->channel[num].code);
    // lyy_printf("name : %s",para->channel[num].name);
    // lyy_printf("number : %s",para->channel[num].channel_number);
    // lyy_printf("channel_code : %s",para->channel[num].channel_code);
    memcpy(infor.wallet,para->channel[num].code,64);
    memcpy(infor.name,para->channel[num].name,64);
    memcpy(infor.id,para->channel[num].channel_number,64);
    memcpy(infor.channel,para->channel[num].channel_code,64);
    LYY_LOGD("code2 : %s",infor.wallet);
    LYY_LOGD("name2 : %s",infor.name);
    LYY_LOGD("number2 : %s",infor.id);
    LYY_LOGD("channel_code2 : %s",infor.channel);
    LYY_LOGD("sizeof(NDJ_CHANNEL_INFOR) : %d",sizeof(NDJ_CHANNEL_INFOR));
    device_ndj_dzy_paytjc_send(id,0x01,&infor.sum,sizeof(NDJ_CHANNEL_INFOR));    
}
/**
 * @brief 
 * 获取套餐信息应答
 * @param id 
 */
void ndj_dzy_get_pay_package_ack(uint8_t id)
{
    PAY_PACKAGES_NDJ_DEF *infor = get_pay_ndj_packages();
    NDJ_PACKAGE_DEF temp = {0};
    if(id <=  0)    return;
    temp.sum = infor->lane_sum;
    memcpy(temp.id,infor->lane[id-1].pkg.id,32);
    temp.seq = infor->lane[id-1].pkg.seq;
    temp.price = infor->lane[id-1].pkg.price;
    memcpy(temp.display_price,infor->lane[id-1].pkg.display_price,32);
    temp.priceValue = infor->lane[id-1].pkg.priceValue;
    memcpy(temp.display_value,infor->lane[id-1].pkg.display_value,32);
    memcpy(temp.unit,infor->lane[id-1].pkg.unit,32);
    strcpy(temp.unit,infor->currency_name);
    LYY_LOGD("sizeof(PAY_PACKAGE_DEF) : %d",sizeof(PAY_PACKAGE_DEF));
    LYY_LOGD("id : %s",temp.id);
    LYY_LOGD("price : %.2f",temp.price);
    LYY_LOGD("sizeof(price):%d = %f",sizeof(temp.price),temp.price);
    LYY_LOGD("unit:%s",temp.unit);
    LYY_LOGD("display_price = %s",temp.display_price);
    device_ndj_dzy_paytjc_send(id,0x02,&temp.sum,sizeof(NDJ_PACKAGE_DEF));    

}
/**
 * @brief 
 * 货道信息应答
 * @param id 
 */
void ndj_dzy_get_lane_ack(uint8_t id)
{
    PLANE_INFOR_TABLE_DEF infor = ndj_net_lane_infor();
    NDJ_LANE_DEF temp = {0};
    if(id == 0) return;
    temp.sum = infor->number;
    memcpy(&temp.play_time,&infor->lane[id-1].play_time,sizeof(NDJ_LANE_DEF)-1);
    LYY_LOGD("sizeof(NDJ_LANE_DEF) : %d",sizeof(NDJ_LANE_DEF));
    LYY_LOGD("play_time : %d",temp.play_time);
    LYY_LOGD("sum : %d",temp.sum);
    LYY_LOGD("status : %d",temp.status);
    LYY_LOGD("status : %d",temp.enable);
    temp.enable = !temp.enable; //主板协议 0 ：启用 1：禁用，所以需要取反
    LYY_LOGD("startup_unit : %d",temp.startup_unit);
    LYY_LOGD("giftName : %s",temp.giftName);
    device_ndj_dzy_paytjc_send(id,0x03,&temp.sum,sizeof(NDJ_LANE_DEF));    
}

/**
 * @brief 
 * 获取动态二维码
 * @param pack 
 * @param len 
 */
void ndj_dzy_get_pay_qrcode(uint8_t *pack,uint16_t len)
{
    (void)len;
    char commodity_id[32]= {0};
    char channel[64] = {0};
    char channel_number[64]= {0};
    if(pack[6] == 0) return ;
    uint8_t lane_id = pack[6];
    ndj_infor_t.lane_id = lane_id;
    ndj_infor_t.dzy_flag = 1;
    memcpy(commodity_id,&pack[7],32);
    memcpy(channel,&pack[7+32],64);
    memcpy(channel_number,&pack[7+32+64],64);
    LYY_LOGD("commodity_id:%s",commodity_id);
    LYY_LOGD("channel:%s",channel);
    LYY_LOGD("channel_number:%s",channel_number);
    LYY_LOGD("lane_id:%d",lane_id);
    event_post_query_dynamic_url_ndj(
                                    commodity_id,
                                    channel,
                                    channel_number,
                                    lane_id,
                                    0);  //获取动态支付二维码   
}
/**
 * @brief 
 * 获取盒子信息
 * @param pack 
 * @param len 
 */
void ndj_dzy_get_box_infor(uint8_t *pack,uint16_t len)
{

    (void)len;
    
    NDJ_DZY_BOX_INFOR_DEF infor = {0};

    infor.csq =  HAL_lGetWifiRssiToCsq();
    strcpy(infor.soft_ver,CFG_APP_VERSION);
    strcpy(infor.hard_ver,CFG_HARD_VERSION);

    strcpy(infor.dev_id,APP_pcGetParamDevId());
    infor.net_status = (lyy_mqtt_get_status() == 1) ? 1:0;
    device_ndj_dzy_paytjc_send(pack[6],0x05,(uint8_t *)infor.soft_ver,sizeof(NDJ_DZY_BOX_INFOR_DEF)); 
}

/**
 * @brief 
 * 获取区域信息
 * @param pack 
 * @param len 
 */
void ndj_dzy_get_region_infor(uint8_t *pack,uint16_t len)
{
    (void)len;
    // #if 0
    INFOR_BUSINESS *business = get_business_infor();
    device_ndj_dzy_paytjc_send(pack[6],0x06,(uint8_t *)business->dev_id,sizeof(INFOR_BUSINESS)); 
    // #endif
}
/**
 * @brief 
 * 获取支付二维码
 * @param id 
 */
void ndj_dzy_get_qrcode_ack(uint8_t id,char *url)
{
    device_ndj_dzy_paytjc_send(id,0x04,(uint8_t *)url,512);
}
/**
 * @brief 
 * dzy 定制协议解析
 * 采用0x07指令
 * @param packet 
 * @param len 
 */
void device_ndj_dzy_packet_parse(uint8_t *packet,uint16_t len)
{
    uint8_t sub_cmd,id;
    sub_cmd = packet[5];
    id = packet[6];
    LYY_LOGD("device_ndj_dzy_packet_parse sub_cmd=%d; id=%d" ,sub_cmd,id);
    switch (sub_cmd)
    {
        case 0x01: //获取渠道信息
        {
            ndj_dzy_get_pay_channel_ack(id,packet[7]);    
        }break;
        case 0x02:
        {
            ndj_dzy_get_pay_package_ack(id);
        }break;
        case 0x03:
        {
            ndj_dzy_get_lane_ack(id);
        }break;
        case 0x04:
        {
            ndj_dzy_get_pay_qrcode(packet,len);
        }break;
        case 0x05:
        {
            ndj_dzy_get_box_infor(packet,len);
        }break;
        case 0x06:
        {
            ndj_dzy_get_region_infor(packet,len);
        }break;
        default:break;
    }
}




/**
 * @brief Get the update event infor object
 * 
 * @return PNDJ_UPDATE_EVENT_DEF 
 */
PNDJ_UPDATE_EVENT_DEF get_update_event_infor(void)
{
    return &ndj_update_event_t;
}
/**
 * @brief 
 * 
 * @param packet 
 * @param len 
 */
void device_ndj_update_lane_rsp(uint8_t *packet,uint16_t len)
{
    (void)len;
    DEVICE_PACK_DEF tDevPack;
    memset(&tDevPack, 0, sizeof(tDevPack));
    //应答
    tDevPack.len = 0x06; 
    tDevPack.cmd = packet[3];
    tDevPack.data[0] = packet[4];
    tDevPack.data[1] = packet[5];
    tDevPack.data[2] = packet[20]; //货道总数
    device_buildPacket(&tDevPack);
}
/**
 * @brief 
 * 设置仓位参数
 * @param id 
 * @param time 
 * @param coins 
 * @param status 
 */
void device_ndj_set_lane_infor(uint8_t id,uint8_t time,uint8_t coins,uint8_t status)
{
    LANE_FORMAT_DEF lane={0};
    LYY_LOGD("device_ndj_set_lane_infor");
    lane.sub_id = 0x01;
    lane.sub_cmd = 0x07;
    lane.id = id;
    lane.status = status;
    lane.time = time;
    lane.coins = coins;
    device_send_cmd_packet(DEVICE_NDJ_CMD, &lane.sub_id,sizeof(LANE_FORMAT_DEF));
}
/**
 * @brief 
 * 
 * @param packet 
 * @param len 
 */
void device_ndj_update_event_rsp(uint8_t *packet,uint16_t len)
{
    (void)len;
    DEVICE_PACK_DEF tDevPack;
    memset(&tDevPack, 0, sizeof(tDevPack));
    //应答
    tDevPack.len = 0x0C; 
    tDevPack.cmd = packet[3];
    tDevPack.data[0] = packet[4];
    tDevPack.data[1] = packet[5];
    tDevPack.data[2] = packet[6];
    memcpy(tDevPack.data+3,packet+7,6);
    device_buildPacket(&tDevPack);
}
/**
 * @brief 
 * 仓位信息上报
 * @param packet 
 * @param len 
 */
void device_ndj_update_laneInfor(uint8_t *packet,uint16_t len)
{
    (void)len;

    PNDJ_UPDATE_EVENT_DEF event_infor = get_update_event_infor();
    device_ndj_update_lane_rsp(packet,len);
    memcpy(&event_infor->lane.sub_id,packet+4,len-6);
}

/**
 * @brief 
 * 单个仓位信息应答
 * @param packet 
 * @param len 
 */
void device_ndj_ack_get_laneInfor(uint8_t *packet,uint16_t len)
{
    (void)len;
    LANE_FORMAT_DEF lane_infor;
    if(ndj_infor_t.flag_get_lane != 1) //有需要查询
    {
        ndj_infor_t.flag_get_lane = 0;
        memcpy(&lane_infor.sub_id,packet+4,sizeof(LANE_FORMAT_DEF));
        thing_service_reply_ndj_lane_get(lane_infor.id,lane_infor.status,lane_infor.coins,lane_infor.time,ndj_infor_t.get_lane_id);
        memset(ndj_infor_t.get_lane_id,0,sizeof(ndj_infor_t.get_lane_id));
    }
}

/**
 * @brief 
 * 设置单个仓位信息应答
 * @param packet 
 * @param len 
 */
void device_ndj_ack_set_laneInfor(uint8_t *packet,uint16_t len)
{

    (void)packet;
    (void)len;

    if(ndj_infor_t.flag_set_lane != 1) //有需要查询
    {
        ndj_infor_t.flag_set_lane  = 0;
        thing_service_reply_ndj_lane_set(1,ndj_infor_t.set_lane_id);
        memset(ndj_infor_t.set_lane_id,0,sizeof(ndj_infor_t.set_lane_id));
    }
}
/**
 * @brief 
 * 启动结果上报
 * @param packet 
 * @param len 
 */
void device_ndj_update_startResult(uint8_t *packet,uint16_t len)
{
    uint8_t lane_id = packet[6]; //仓位ID
    if (lane_id >= 1)
        lane_id -= 1;
    PNDJ_UPDATE_EVENT_DEF infor = get_update_event_infor();
    device_ndj_update_event_rsp(packet,len);
    if(memcmp(infor->start[lane_id].start_result_cmd.pack_id,packet+7,6)==0) //指令唯一码一样
        return ;
    memcpy(&infor->start[lane_id].start_result_cmd.sub_id,packet+4,sizeof(NDJ_START_RESULT_CMD_DEF)); //缓存数据
    infor->start[lane_id].flag = 1;
}
/**
 * @brief 
 * 故障上报
 * @param packet 
 * @param len 
 */
void device_ndj_update_fault(uint8_t *packet,uint16_t len)
{
    PNDJ_UPDATE_EVENT_DEF event_infor = get_update_event_infor();
    device_ndj_update_event_rsp(packet,len);
    if(memcmp(event_infor->fault.pack_id,packet+7,6)==0) //指令唯一码一样
        return ;
    memcpy(&event_infor->fault.sub_id,packet+4,len-6);
    event_infor->fault.flag = 1;
     
}
/**
 * @brief 
 * 账目上报
 * @param packet 
 * @param len 
 */
void device_ndj_update_acc(uint8_t *packet,uint16_t len)
{
    PNDJ_UPDATE_EVENT_DEF event_infor = get_update_event_infor();
    device_ndj_update_event_rsp(packet,len);
    if(memcmp(event_infor->acc.pack_id,packet+7,6)==0) //指令唯一码一样
        return ;
    memcpy(&event_infor->acc.sub_id,packet+4,len-6);
    event_infor->acc.flag = 1;
}
/**
 * @brief 
 * 扭蛋机上报信息
 */
void device_ndj_update_infor(void)
{
    char order[20] = {0};
    uint8_t i = 0;
    PLANE_INFOR_TABLE_DEF lane = ndj_net_lane_infor();
    PNDJ_UPDATE_EVENT_DEF infor = get_update_event_infor();
    utils_lhexToString(order,infor->acc.pack_id,6);
    //账目
    if(infor->acc.flag == 1) //上报账目信息
    {
        infor->acc.flag = 0;
        LYY_LOGD("post account gift:%d,sum_gift:%d",infor->acc.gift_inc,infor->acc.gift_sum);
        event_post_account_ndj(
                            infor->acc.coin_inc,
                            infor->acc.coin_sum,
                            infor->acc.gift_inc,
                            infor->acc.gift_sum,
                            infor->acc.lane_id,
                            order);    
    } 
    vTaskDelay(100 / portTICK_PERIOD_MS);   
    //故障
    if(infor->fault.flag == 1)
    {
        infor->fault.flag = 0;
        event_post_board_err_ndj(infor->fault.fault,infor->fault.lane_id,0);
    } 
    vTaskDelay(100 / portTICK_PERIOD_MS);  
    //启动结果
    for(i=0;i<lane->number;i++)
    {
        if(infor->start[i].flag == 1)
        {
            LYY_LOGD("start result:%d",infor->start[i].start_result_cmd.coins);
            infor->start[i].flag = 0;
            event_post_start_result_ndj(infor->start[i].start_result_cmd.coins,infor->start[i].start_result_cmd.lane_id,0,infor->start[i].order);
            vTaskDelay(500 / portTICK_PERIOD_MS);  
        }
    }
    
    //货道信息设置
    // LYY_LOGD("lane->number = %d",lane->number);
    for(i=0;i<lane->number;i++)
    {
        if(lane->flag[i] == 1)
        {
            LYY_LOGD("lane->flag[%d] = %d",i,lane->flag[i]);
            ndj_infor_t.flag_set_lane = 1;
            lane->flag[i] = 0;
            device_ndj_set_lane_infor(
                                        lane->lane[i].position_x,
                                        lane->lane[i].play_time,
                                        lane->lane[i].startup_unit,
                                        !lane->lane[i].enable);  // 后台TRUE：在线，FASLE：离线，而主板协议 0 ：在线  1：离线 所以设置时需要取反
            vTaskDelay(500 / portTICK_PERIOD_MS);  
        }
    }
}

/**
 * @brief 
 * 
 * @param lane_id 
 */
void get_ndj_pay_cmd_id(uint8_t lane_id,uint8_t *cmd_id)
{
    uint8_t i =0;
    for(i=0;i<6;i++)
    {
        g_xlyyHwMark.ndj_cmd_id[lane_id][i]++;
    }
    memcpy(cmd_id,g_xlyyHwMark.ndj_cmd_id[lane_id],6);
}

/**
 * @brief 
 * 支付申请 -- 扭蛋机
 * @param pay_id 
 * @param sub_id 
 * @param id 
 * @return uint8_t 
 * > 0 成功
 * =0 失败
 */
uint8_t device_ndj_pay_apply(uint8_t sub_id,uint8_t id)
{
    NDJ_PAY_APPLY_DEF pay_apply = {0};
    pay_apply.sub_id = sub_id;
    pay_apply.sub_cmd = 0x03;
    pay_apply.id = id;
    g_xlyyHwMark.ndj_apply_id = id; //保存id
    get_ndj_pay_cmd_id(id,pay_apply.pay_order);

    device_send_cmd_packet(DEVICE_NDJ_CMD , &pay_apply.sub_id,9);

    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 3000); //等待主板启动应答
}
/**
 * @brief 
 * ndj 启动
 * @param pay_id 
 * @param sub_id 
 * @param id 
 * @param number 
 * @return uint8_t 
 * > 0 成功
 * =0 失败
 */
uint8_t deivce_ndj_pay_start(uint8_t sub_id,uint8_t id,uint16_t number)
{
    // uint8_t ret = 0; 
    NDJ_PAY_DEF pay = {0};
    pay.sub_id = sub_id;
    pay.sub_cmd = 0x04;
    pay.id = id;
    g_xlyyHwMark.ndj_start_id = id;
    pay.number = number;

    get_ndj_pay_cmd_id(id,pay.pay_order);

    device_send_cmd_packet(DEVICE_NDJ_CMD , &pay.sub_id,sizeof(NDJ_PAY_DEF));
        
    return xSemaphoreTake(g_xlyyHwMark.pxPayApply, 3000); //等待主板启动应答

}

/**
 * @brief 
 * 
 * @param sub_id 
 * @param id 
 * @return uint8_t 
 */
uint8_t deivce_ndj_lane_infor_get(uint8_t sub_id,uint8_t id)
{
    NDJ_GET_CMD_DEF get;
    get.sub_id = sub_id;
    get.sub_cmd = 0x06;
    get.lane_id = id;
    device_send_cmd_packet(DEVICE_NDJ_CMD , &get.sub_id,sizeof(NDJ_GET_CMD_DEF));
    return  1;  
}
/**
 * @brief 
 * 设置仓位信息
 * @param sub_id 
 * @param id 
 * @param status 
 * @param time 
 * @param coins 
 * @return uint8_t 
 */
uint8_t deivce_ndj_lane_infor_set(uint8_t sub_id,uint8_t id,uint8_t status,uint8_t time,uint8_t coins)
{
    LANE_FORMAT_DEF set;
    set.sub_id  = sub_id;
    set.sub_cmd = 0x07;
    set.id      = id;
    set.time    = time;
    set.coins   = coins;
    set.status  = status;
    device_send_cmd_packet(DEVICE_NDJ_CMD , &set.sub_id,sizeof(LANE_FORMAT_DEF));
    return  1;  
}

/**
 * @brief 
 * 
 * @param packet 
 * @param len 
 */
void device_ndj_packet_parse(uint8_t *packet,uint16_t len)
{
    uint8_t sub_cmd = packet[5];
    uint8_t sub_id  = packet[6]; //仓位
    switch(sub_cmd)
    {
        case 0x01: //上报仓位信息
        {
            device_ndj_update_laneInfor(packet,len);  
        }break;
        case 0x03: //支付申请
        {
            LYY_LOGD("-ndj- pay apply ack sub_id:%d ,backup_id:%d ",sub_id,g_xlyyHwMark.ndj_apply_id);
            if(g_xlyyHwMark.ucPaySem && g_xlyyHwMark.ndj_apply_id == sub_id)
            {
                g_xlyyHwMark.applyResult= packet[13];
                LYY_LOGD("apply_result:%d",g_xlyyHwMark.applyResult);
    
                xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                g_xlyyHwMark.ucPaySem = 0x00;
 
            }    
        }break;
        case 0x04: //支付
        {
            LYY_LOGD("-ndj- pay start ack sub_id:%d ,backup_id:%d ",sub_id,g_xlyyHwMark.ndj_start_id);
            if(g_xlyyHwMark.ucPaySem && g_xlyyHwMark.ndj_start_id == sub_id)
            {
                g_xlyyHwMark.startResult = data_arrayTolittle16(packet+13);
                LYY_LOGD("ndj[start_ret]:%d",g_xlyyHwMark.startResult);
                if(g_xlyyHwMark.ucPaySem)
                {
                    xSemaphoreGive(g_xlyyHwMark.pxPayApply);
                    g_xlyyHwMark.ucPaySem = 0x00;
                }
            }     
        }break;
        case 0x05: //支付结果上报
        {
            LYY_LOGD("-ndj- start result");
            device_ndj_update_startResult(packet,len);
        }break;
        case 0x06: //查询仓位信息
        {
            LYY_LOGD("-ndj- get parameter");
            device_ndj_ack_get_laneInfor(packet,len);
        }break;
        case 0x07: //设置仓位信息
        {
            LYY_LOGD("-ndj- set lane ack");
            device_ndj_ack_set_laneInfor(packet,len);
        }break;
        case 0x0A: //故障上报
        {
            LYY_LOGD("-ndj- fault update");
            device_ndj_update_fault(packet,len);        
        }break;
        case 0x08: //账目信息
        {
            LYY_LOGD("-ndj- acc update");
            device_ndj_update_acc(packet,len);    
        }break;
        default: break;
    }
}
/**
 * @brief 
 * 
 */
void ndj_value_init(void)
{
    PNDJ_UPDATE_EVENT_DEF event_infor = get_update_event_infor();
    memset(event_infor->acc.pack_id,0x12,sizeof(event_infor->acc.pack_id));
    memset(event_infor->fault.pack_id,0x14,sizeof(event_infor->fault.pack_id));
    for(uint8_t i = 0;i<NDJ_LANE_MAX;i++)
        memset(event_infor->start[i].start_result_cmd.pack_id,0x15,sizeof(event_infor->start[i].start_result_cmd.pack_id));
}
/**
 * @brief 
 * 
 * @param arg 
 */
void thread_ndj_update_infor(void *arg)
{
    (void)arg;
    while(1)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);  
        if(lyy_mqtt_get_status() == 1)
            device_ndj_update_infor();
    }
}
/**
 * @brief 
 * 
 */
void lyy_device_ndj_init(void)
{

    vRandomGet((char *)g_xlyyHwMark.ndj_cmd_id, sizeof(g_xlyyHwMark.ndj_cmd_id));

#define TJC_THREAD_NAME             "NDJ_INFOR"
#define TJC_THREAD_STACK            (1024 * 4)

    xTaskCreate(    thread_ndj_update_infor, 
                    TJC_THREAD_NAME, 
                    TJC_THREAD_STACK, 
                    NULL, 
                    5,
                    NULL); 

}





