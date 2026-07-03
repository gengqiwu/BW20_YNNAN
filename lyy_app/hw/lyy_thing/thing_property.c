/*
 * @author: e.he
 * @Date: 2024-06-20 17:42:36
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-09 17:40:54
 * @Description: 
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#include "utils.h"
#include "cJSON.h"

#include "HAL_Log.h"
#include "HAL_Uart.h"
#include "HAL_System.h"

#include "APP_Param.h"
#include "APP_Detect.h"

#include "thing_iot.h"
#include "thing_init.h"
#include "thing_event.h"
#include "thing_service.h"
#include "thing_property.h"

#include "lyy_ndj.h"
#include "lyy_tjc.h"
#include "lyy_pic.h"
#include "lyy_para.h"
#include "lyy_start.h"
#include "lyy_yunanshj.h"

/*属性设置*/
int property_write_uart_infor(cJSON* params);
int property_write_pay_pkgs_ndj(cJSON* params);
int property_write_pay_pkgs(cJSON* params);
int property_write_pay_channel(cJSON* params);
int property_write_dev_infor(cJSON* params);
int property_write_pulse_param(cJSON* params);
int property_write_ndj_lane(cJSON* params);
int property_write_fault_list(cJSON* params);
int property_write_picture_screen(cJSON* params);
int property_write_vending_drop_detection(cJSON* params);

/*属性读取*/
int property_read_brt_param(cJSON* reply);
int property_read_pulse_para(cJSON* reply);
int property_read_infor_net(cJSON* reply);
int property_read_login_flag(cJSON* reply);
int property_read_dev_infor(cJSON* reply);
/**
 * @brief 
 * 属性操作
 */
const PROPER_METHOD_DEF method_property[] = 
{
    {"uart_brt",                property_write_uart_infor,      property_read_brt_param },                 //串口参数
    {"position_pay_package",    property_write_pay_pkgs_ndj,    NULL                    },                 //支付套餐
    {"pay_package",             property_write_pay_pkgs,        NULL                    },                 //其他套餐
    {"pay_channel",             property_write_pay_channel,     NULL                    },                 //支付渠道 
    {"equipment_info",          property_write_dev_infor,       NULL                    },                 //业务信息
    {"pulse_para",              property_write_pulse_param,     property_read_pulse_para},                  //脉冲参数
    {"position_info",           property_write_ndj_lane,        NULL                    },     
    {"customize_errors",        property_write_fault_list,      NULL                    },     
    {"infor_net",               NULL,                           property_read_infor_net },        
    {"login_flag",              NULL,                           property_read_login_flag},        
    {"dev_infor",               NULL,                           property_read_dev_infor },         
    {"network_level",           NULL,                           NULL                    },                          
    {"attr_container_layout",   property_write_picture_screen, NULL                     },
    {"vending_drop_detection",  property_write_vending_drop_detection, NULL             },
};



void login_flag_packet(cJSON *login,uint16_t login_flag,uint8_t result,char *message);


DEV_LOGIN_INFOR_DEF     dev_login_infor_t;
/**
 * @brief 
 * 属性设置
 * @param params 
 */
void thing_property_set(cJSON* params)
{

    if(!params)
    {
        return ;
    }

    cJSON* object               = NULL;
    uint8_t i                   = 0;
    int res                     = -1;
    PROPER_METHOD_DEF *method   = NULL;
    char id[6]                  = {0x00,};
    char id_str[15]             = {0x00,};

    for(i=0; i<(sizeof(method_property)/sizeof(method_property[0])); i++)
    {
        method = (PROPER_METHOD_DEF *)&method_property[i];
        object = cJSON_GetObjectItem(params, method->method);
        if(object != NULL)
        {
            if(method->pro_write != NULL)
                res = method->pro_write(object);
            else
            {
                LYY_LOGD("pro_write[%s] is null",method->method);
            }
            break;
        }
    }
    //id-相当于指令唯一码
    vRandomGet(id, sizeof(id));
    utils_lhexToString(id_str, (const unsigned char*)id, sizeof(id));
    thing_property_set_reply(((res>=0)?"success":"fail"),id_str);

}


/**
 * @description: 属性读取
 * @param {*}
 * @return 
 */
void thing_property_read(cJSON* params)
{
    uint8_t i = 0, j = 0;
    PROPER_METHOD_DEF *method   = NULL;
	int sub_size                = 0;
    char *para_name             = NULL;

    sub_size = cJSON_GetArraySize(params);	//获取数组成员个数 
    LYY_LOGD("sub_size %d", sub_size);

    
    for(i=0; i<sub_size; i++)
    {
        para_name = cJSON_GetArrayItem(params, i)->valuestring;
        LYY_LOGD(" %s ",para_name);//解析数组中的字符串

        for(j=0; j<(sizeof(method_property)/sizeof(method_property[0])); j++)
        {
            method = (PROPER_METHOD_DEF *)&method_property[j];
            if(0 == memcmp(method->method, para_name, strlen(para_name)))
            {

                if(method->pro_read != NULL)
                {
                    cJSON *replyParams  = cJSON_CreateObject();

                    if(!replyParams) {
                        break ;
                    }

                    if(method->pro_read(replyParams) >= 0) 
                        property_read_reply(replyParams,"success");
                    else{

                        if(replyParams) {
                            cJSON_Delete(replyParams);
                        }
                        
                    }   

                    break;

                }
            }
        }
    }
  
}

/**
 * @description: 波特率设置
 * @param {*}
 * @return {*}
 */
int property_write_uart_infor(cJSON* params)
{
    LYY_LOGD("set uart brt %s", params->valuestring);

    uint32_t ulBaundRate = atoi(params->valuestring);
    
    if(APP_ulGetParamBaudRate() != ulBaundRate)
    {
        APP_lParamSetBaundRate(ulBaundRate);

        HAL_vUartSetParam(HAL_UART_MAIN_BOARD, ulBaundRate);
    }

    return 1;
}


/**
 * @brief 
 * 更新套餐
 * @param pkgs 
 * @param body 
 * @return int 
 */
void pay_package_update(void *package,cJSON *body)
{

    cJSON *pkgs_data = NULL;

    PAY_PACKAGE_DEF *pkgs = (PAY_PACKAGE_DEF *)package;
    pkgs_data = cJSON_GetObjectItem(body, "id");

    if(pkgs_data != NULL && pkgs_data->type == cJSON_String)
    {
        strcpy(pkgs->id,pkgs_data->valuestring);
        LYY_LOGD("pkgs->id=%s",pkgs->id);
    }    

    //排序
    pkgs_data = cJSON_GetObjectItem(body, "seq");
    if(pkgs_data != NULL && pkgs_data->type == cJSON_Number)
    {
        pkgs->seq = pkgs_data->valueint;
        LYY_LOGD("pkgs->seq=%d", pkgs->seq);
    } 

    //金额
    pkgs_data = cJSON_GetObjectItem(body, "price");
    if(pkgs_data != NULL)
        pkgs->price = pkgs_data->valuedouble;
    LYY_LOGD("pkgs->price=%.2f", pkgs->price);

    //展示金额
    pkgs_data = cJSON_GetObjectItem(body, "displayPrice"); 
    if(pkgs_data != NULL)
        strcpy(pkgs->display_price,pkgs_data->valuestring);
    LYY_LOGD("pkgs->display_price=%s",pkgs->display_price);

    //商品价值
    pkgs_data = cJSON_GetObjectItem(body, "priceValue");
    if(pkgs_data != NULL)
        pkgs->priceValue = pkgs_data->valuedouble;
    LYY_LOGD("pkgs->priceValue=%.2f",pkgs->priceValue);

    //商品展示价值
    pkgs_data = cJSON_GetObjectItem(body, "displayPriceValue");
    if(pkgs_data != NULL && pkgs_data->type == cJSON_String)
    {
        strcpy(pkgs->display_value, pkgs_data->valuestring);
        LYY_LOGD("pkgs->display_value=%s", pkgs->display_value);
    } 

    //商品价值单位
    pkgs_data = cJSON_GetObjectItem(body, "priceValueUnitDesc");
    if(pkgs_data != NULL && pkgs_data->type == cJSON_String)
    {
        strcpy(pkgs->unit, pkgs_data->valuestring);
        LYY_LOGD("pkgs->UnitDesc=%s", pkgs->unit);
    }


    /* 未使用到的参数，节约资源 */
#if 0

    //sku编码
    pkgs_data = cJSON_GetObjectItem(body, "sku");
    if(pkgs_data != NULL && pkgs_data->type == cJSON_String)
    {
        strcpy(pkgs->sku,pkgs_data->valuestring);
        LYY_LOGD("pkgs->sku=%s", pkgs->sku);
    }
    //商品名称
    pkgs_data = cJSON_GetObjectItem(body, "name");
    if(pkgs_data != NULL && pkgs_data->type == cJSON_String)
    {
        strcpy(pkgs->name,pkgs_data->valuestring);
        LYY_LOGD("pkgs->name=%s",pkgs->name);
    }   

    //商品价值单位翻译
    pkgs_data = cJSON_GetObjectItem(body, "priceValueUnitDescribe");
    if(pkgs_data != NULL && pkgs_data->type == cJSON_String)
    {
        strcpy(pkgs->translate, pkgs_data->valuestring);
        LYY_LOGD("pkgs->translate=%s",pkgs->translate);
    }

#endif

}


/**
 * @description: 支付套餐设置
 * @param {*}
 * @return {*}
 */
int property_write_pay_pkgs_ndj(cJSON* params)
{   
    //ndj_add
    uint8_t i   = 0;
    uint8_t lane_id = 0;

    PAY_PACKAGES_NDJ_DEF *ndj_pkgs = get_pay_ndj_packages();
    cJSON *lane_data = NULL;
    cJSON *package = NULL;
    cJSON *cJsonData = NULL;
    cJSON *data = NULL;
    if(params == NULL)
    {
        LYY_LOGD("property_write_pay_pkgs_ndj is null");
        return -3;    
    }
    if(params->type == cJSON_String)
    {
        // LYY_LOGD("ndj_pay_package %s", params->valuestring);
        LYY_LOGD("property_write_pay_pkgs_ndj type is string");
    }
    LYY_LOGD("property_write_pay_pkgs_ndj");
    cJsonData = cJSON_Parse(params->valuestring);
    if(cJsonData == NULL)   
    {
        LYY_LOGD("cJSON_Parse fail");
        return -2;
    }
    //货币 名称
    data = cJSON_GetObjectItem(cJsonData, "currencyCode");
    if(data != NULL && data->type == cJSON_String)
    {
        memset(ndj_pkgs->currency_name, 0x00, sizeof(ndj_pkgs->currency_name));
        strcpy(ndj_pkgs->currency_name, data->valuestring);
        LYY_LOGD("ndj_pkgs->currency_name: %s", ndj_pkgs->currency_name);
    }

    // 货币 符号
    data = cJSON_GetObjectItem(cJsonData, "currencySymbol");
    if(data != NULL && data->type == cJSON_String)
    {
        memset(ndj_pkgs->currency_symbol, 0x00, sizeof(ndj_pkgs->currency_symbol));
        strcpy(ndj_pkgs->currency_symbol, data->valuestring);
        LYY_LOGD("ndj_pkgs->currency_symbol: %s", ndj_pkgs->currency_symbol);
    }
    data = cJSON_GetObjectItem(cJsonData, "positionList");
    if(data == NULL)
    {
        cJSON_Delete(cJsonData);//清除结构体
        return -3;
    }
    ndj_pkgs->lane_sum = cJSON_GetArraySize(data);	//获取数组成员个数 
    LYY_LOGD("ndj_pkgs->lane_sum: %d", ndj_pkgs->lane_sum);
    if(ndj_pkgs->lane_sum == 0)//无套餐
    {
        LYY_LOGD("no lane no package");
        //显示无套餐
        cJSON_Delete(cJsonData);//清除结构体
        return -4; 
    } 
 
    for(i=0; i< ndj_pkgs->lane_sum; i++)
    {
        lane_data = cJSON_GetObjectItem(cJSON_GetArrayItem(data, i), "positionX");
        if(lane_data != NULL && lane_data->type == cJSON_Number && (lane_data->valueint >= 1))
        {
            lane_id = lane_data->valueint -1;
            if(lane_id >= NDJ_LANE_MAX) continue ; // 仓位ID过大
            ndj_pkgs->lane[lane_id].x = lane_data->valueint;
            LYY_LOGD("lane[%d].x=%d", lane_id, lane_data->valueint);
        }
        else continue;  //没有仓位ID跳过后面部分

        lane_data = cJSON_GetObjectItem(cJSON_GetArrayItem(data, i), "positionY");
        if(lane_data != NULL && lane_data->type == cJSON_Number)
        {
            LYY_LOGD("lane[%d].y=%d", lane_id, lane_data->valueint);
            ndj_pkgs->lane[lane_id].y = lane_data->valueint;
        }

        package = cJSON_GetObjectItem(cJSON_GetArrayItem(data, i), "consumptionPackage");
        ndj_pkgs->lane[lane_id].number = cJSON_GetArraySize(package);	//获取数组成员个数 
        if(ndj_pkgs->lane[lane_id].number == 0) //无套餐
        {
            LYY_LOGD("lane[%d] :no package",lane_id); 
            continue ;  //没有仓位ID跳过后面部分  
        }
        LYY_LOGD("para->lane[%d].number = %d",lane_id,ndj_pkgs->lane[lane_id].number);
        #if 0 // 暂时不支付多个number todo
        for(j=0;j<ndj_pkgs->lane[lane_id].number;j++)
        {
            LYY_LOGD("ndj_pkgs->lane[%d].pkg[%d]",lane_id,j);
            pay_package_update(&ndj_pkgs->lane[lane_id].pkg[j],cJSON_GetArrayItem(package, j));
        }
        #endif
        LYY_LOGD("ndj_pkgs->lane[%d].pkg",lane_id);
        pay_package_update(&ndj_pkgs->lane[lane_id].pkg,cJSON_GetArrayItem(package, 0));
    }

    cJSON_Delete(cJsonData);//清除结构体
    vTaskDelay(500 / portTICK_PERIOD_MS);   

    lyy_tjc_page_package();

    return 1;

}


/**
 * @description: 支付套餐设置
 * @param {*}
 * @return {*}
 */
int property_write_pay_pkgs(cJSON* params)
{

    uint8_t i = 0;

    PAY_PACKAGES_DEF *pkgs_t = get_pay_packages();
    cJSON *data = NULL;
    cJSON *cJsonData = NULL;
    
    if(params->type == cJSON_String)
    {
        LYY_LOGD("ndj_pay_package %s", params->valuestring);
    }

    cJsonData = cJSON_Parse(params->valuestring);
    if(cJsonData == NULL)   
    {
        LYY_LOGD("cJSON_Parse fail");
        return -2;
    }
    //货币 名称
    data = cJSON_GetObjectItem(cJsonData, "currencyCode");
    if(data != NULL && data->type == cJSON_String)
    {
        memset(pkgs_t->currency_name, 0x00, sizeof(pkgs_t->currency_name));
        strcpy(pkgs_t->currency_name, data->valuestring);
        LYY_LOGD("ndj_pkgs->currency_name: %s", pkgs_t->currency_name);
    }

    // 货币 符号
    data = cJSON_GetObjectItem(cJsonData, "currencySymbol");
    if(data != NULL && data->type == cJSON_String)
    {
        memset(pkgs_t->currency_symbol, 0x00, sizeof(pkgs_t->currency_symbol));
        strcpy(pkgs_t->currency_symbol, data->valuestring);
        LYY_LOGD("ndj_pkgs->currency_symbol: %s", pkgs_t->currency_symbol);
    }

    data = cJSON_GetObjectItem(cJsonData, "consumptionPackage");
    if(data == NULL)
    {
        cJSON_Delete(cJsonData);//清除结构体
        return -3;
    }
    pkgs_t->sum = cJSON_GetArraySize(data);	//获取数组成员个数 
    LYY_LOGD("pkgs_t->sum: %d", pkgs_t->sum);
    if(pkgs_t->sum == 0)//无套餐
    {
        LYY_LOGD("no lane no package");
        //显示无套餐
        cJSON_Delete(cJsonData);//清除结构体
        return -4; 
    } 
    if(pkgs_t->sum > NDJ_LANE_MAX)  // 只获取6个套餐
    {
        pkgs_t->sum = NDJ_LANE_MAX;
    }
    for(i=0; i<pkgs_t->sum; i++)  //存储套餐从1开始  实际套餐名称从1开始
    {
        LYY_LOGD("ndj_pkgs.pkg[%d]",i);
        pay_package_update(&pkgs_t->pkg[i],cJSON_GetArrayItem(data, i));
    }
    if(cJsonData != NULL) cJSON_Delete(cJsonData);//清除结构体
    
    vTaskDelay(500 / portTICK_PERIOD_MS); 

    lyy_tjc_page_package();


    return 1;
}


/**
 * @brief 
 * 支付渠道更新
 * @param chl 
 * @param body 
 */
void pay_channel_update(PAY_CHANNEL_DEF *chl,cJSON* body)
{


    //钱包编号
    cJSON* channel_t = NULL;
    channel_t = cJSON_GetObjectItem(body, "code");
    if(channel_t != NULL && channel_t->type == cJSON_String)
    {
        memset(chl->code, 0x00, sizeof(chl->code));
        strcpy(chl->code, channel_t->valuestring);
        LYY_LOGD("chl->code=%s",chl->code);
        
    }

    //渠道编号
    channel_t = cJSON_GetObjectItem(body, "channelNumber");
    if(channel_t != NULL && channel_t->type == cJSON_String)
    {
        memset(chl->channel_number, 0x00, sizeof(chl->channel_number));
        LYY_LOGD("chl->number=%s",channel_t->valuestring);
        strcpy(chl->channel_number, channel_t->valuestring);
    }
    /*渠道编码*/
    channel_t = cJSON_GetObjectItem(body, "channelCode");
    if(channel_t != NULL && channel_t->type == cJSON_String)
    {
        memset(chl->channel_code, 0x00, sizeof(chl->channel_code));
        LYY_LOGD("chl->channel_code=%s", channel_t->valuestring);
        strcpy(chl->channel_code, channel_t->valuestring);
    }

    //区域信息
    channel_t = cJSON_GetObjectItem(body, "locale");
    if(channel_t != NULL && channel_t->type == cJSON_String)
    {
        memset(chl->region, 0x00, sizeof(chl->region));
        LYY_LOGD("chl->region=%s", channel_t->valuestring);
        strcpy(chl->region, channel_t->valuestring);
    }

    // 钱包类型
    channel_t = cJSON_GetObjectItem(body, "paymentMethodTypeEnum");
    if (channel_t != NULL && channel_t->type == cJSON_String)
    {
        memset(chl->pay_type, 0x00, sizeof(chl->pay_type));
        LYY_LOGD("chl->pay_type=%s", channel_t->valuestring);
        strcpy(chl->pay_type, channel_t->valuestring);
    }

#if 0 

    //钱包名称
    channel_t = cJSON_GetObjectItem(body, "name");
    if(channel_t != NULL)
    {
        memset(chl->name, 0x00, sizeof(chl->name));
        strcpy(chl->name, channel_t->valuestring);
        LYY_LOGD("chl->name=%s", chl->name);
        
    }
    //图标url
    channel_t = cJSON_GetObjectItem(body, "icon");
    if(channel_t != NULL && channel_t->type == cJSON_String)
    {
        memset(chl->icon, 0x00, sizeof(chl->icon));
        LYY_LOGD("chl->icon=%s",channel_t->valuestring);
        if(strlen(channel_t->valuestring)<sizeof(chl->icon)) //图标url太长则放弃
            strcpy(chl->icon, channel_t->valuestring);
        else 
            strcpy(chl->icon, "url is too long");
    }

    //背景编码
    channel_t = cJSON_GetObjectItem(body, "bannerCode");
    if(channel_t != NULL)
    {
        memset(chl->bg_code, 0x00, sizeof(chl->bg_code));
        LYY_LOGD("chl->bannerCode=%s", channel_t->valuestring);
        strcpy(chl->bg_code, channel_t->valuestring);
    }

    //背景url
    channel_t = cJSON_GetObjectItem(body, "bannerLink");
    if(channel_t != NULL)
    {
        memset(chl->bg_url, 0x00, sizeof(chl->bg_url));
        LYY_LOGD("chl->bg_url=%s",channel_t->valuestring);
        if(strlen(channel_t->valuestring)<sizeof(chl->bg_url)) //图标url太长则放弃
            strcpy(chl->bg_url, channel_t->valuestring);
        else 
            strcpy(chl->bg_url, "url is too long");
    }

#endif

}

/**
 * @brief 
 * 支付渠道
 * @param params 
 * @return int 
 */
int property_write_pay_channel(cJSON *params)
{

    PAY_PARAM_DEF *channel_t = get_pay_param();

    cJSON *data     = NULL;
    uint8_t  i = 0;
    cJSON* cJsonData = NULL;
    data = cJSON_GetObjectItem(params, "pay_channel_list");

    if((data != NULL) && (data->type == cJSON_String))
    {

        LYY_LOGD("string=%s", data->valuestring);
        //获取
        cJsonData =  cJSON_Parse(data->valuestring);
        if(cJsonData == NULL)   
        {
            LYY_LOGD("cJSON_Parse fail");
            return -2;
        }
        channel_t->chnl_sum = cJSON_GetArraySize(cJsonData);	//获取数组成员个数 

        //ndj_add
        set_channel_sum(channel_t->chnl_sum);

        LYY_LOGD("=============== channel_t->chnl_sum %d", channel_t->chnl_sum);
        for(i=0; i<channel_t->chnl_sum; i++)
        {
            if(i >= PAY_CHANNEL_MAX) //超过范围
                break;
            pay_channel_update(&channel_t->channel[i],cJSON_GetArrayItem(cJsonData, i));
        }

        tjc_get_pay_channel_enable(); //更新了渠道信息


        cJSON_Delete(cJsonData);//清除结构体

    }
    else
    {
        LYY_LOGD("get pay_channel_list fail");
    }

    return 1;
}


/**
 * @description: 设备业务信息
 * @param {*}
 * @return {*}
 */
int property_write_dev_infor(cJSON* params)
{

    PAY_PARAM_DEF *dev_info_t = get_pay_param();

    cJSON *data = NULL;

    data = cJSON_GetObjectItem(params, "bind_state");
    if(data != NULL )
    {
        dev_info_t->business.bind_state = data->type==cJSON_True ? 0x01:0x00;
        LYY_LOGD("bind_state %d", dev_info_t->business.bind_state);

        if(dev_info_t->business.bind_state == 0x00)
            tjc_infor_t.get_regist_flag = 0x01;

    }

    data = cJSON_GetObjectItem(params, "equipment_num");
    if(data != NULL && data->type == cJSON_String)
    {
        memset(dev_info_t->business.dev_id, 0x00, sizeof(dev_info_t->business.dev_id));
        LYY_LOGD("dev_id %s", data->valuestring);
        strcpy(dev_info_t->business.dev_id, data->valuestring);
    }
    data = cJSON_GetObjectItem(params, "region");
    if(data != NULL && data->type == cJSON_String)
    {
        memset(dev_info_t->business.region, 0x00, sizeof(dev_info_t->business.region));
        LYY_LOGD("region %s", data->valuestring);
        strcpy(dev_info_t->business.region, data->valuestring);
    }

    data = cJSON_GetObjectItem(params, "language");
    if(data != NULL && data->type == cJSON_String)
    {
        memset(dev_info_t->business.language, 0x00, sizeof(dev_info_t->business.language));
        LYY_LOGD("language %s", data->valuestring);
        strcpy(dev_info_t->business.language, data->valuestring);
    }

    return 1;

}

/**
 * @description: 脉冲参数设置
 * @param {*}
 * @return {*}
 */
int property_write_pulse_param(cJSON* params)
{

    cJSON *data                 = NULL;
    APP_PULSE_T  *pxPulseInfo   = APP_pxlGetParamePulse();

    data = cJSON_GetObjectItem(params, "gap");
    if(data == NULL)    return -1;
    pxPulseInfo->ulPulseSpace = data->valueint;

    data = cJSON_GetObjectItem(params, "width");
    if(data == NULL)    return -1;
    pxPulseInfo->ulPulseWidth = data->valueint;

    data = cJSON_GetObjectItem(params, "output_type");
    if(data == NULL)    return -1;
    if(strstr(data->valuestring,"level"))
    {
        pxPulseInfo->ulPulsetype = START_TYPE_LEVEL;
    }else
    {
        pxPulseInfo->ulPulsetype = START_TYPE_COIN;
    }

    APP_lSetParamUpdate();

    
    LYY_LOGD(">>> update gap : [%d] width: [%d] type[%d]", 
                            pxPulseInfo->ulPulseSpace, 
                            pxPulseInfo->ulPulseWidth,
                            pxPulseInfo->ulPulsetype);
    return 0x00;

}

/**
 * @brief 
 * 
 * @param param 
 * @param body 
 */
void ndj_lane_param_update(LANE_INFOR_TABLE_DEF *lane, cJSON *body,uint8_t cnt)
{

    //ndj_add
    uint8_t lane_id = 0;
    cJSON *data = cJSON_GetArrayItem(body, cnt);
    if(data == NULL || data->type != cJSON_String)
    {
        LYY_LOGD("lane data is error"); 
        return ;   
    }
    LYY_LOGD("ndj_lane: %s", data->valuestring);      
    cJSON *cJsonData = cJSON_Parse(data->valuestring);
    if(cJsonData == NULL)   
    {
        LYY_LOGD("cJSON_Parse fail");
        return ;
    }
    cJSON *temp = cJSON_GetObjectItem(cJsonData, "positionX");
    if(temp != NULL && temp->type == cJSON_Number && (temp->valueint >= 1))
    {
        lane_id = temp->valueint - 1;
        if(lane_id >= NDJ_LANE_MAX)
        {
            cJSON_Delete(cJsonData);
            return ; // 仓位ID过大
        }         
        lane->lane[lane_id].position_x = temp->valueint ;
        LYY_LOGD("lane[%d].position_x = %d",lane_id,lane->lane[lane_id].position_x);
        if(lane_id >= NDJ_LANE_MAX)  
        {
            cJSON_Delete(cJsonData);
            return ;
        }
    }
    else  
    {
        cJSON_Delete(cJsonData);
        return ;
    }
    
    temp = cJSON_GetObjectItem(cJsonData, "playTime");
    if(temp != NULL && temp->type == cJSON_Number)
    {
        lane->lane[lane_id].play_time = temp->valueint;
        LYY_LOGD("lane[%d].play_time = %d",lane_id,lane->lane[lane_id].play_time);
    }
    temp = cJSON_GetObjectItem(cJsonData, "status");
    if(temp != NULL && temp->type == cJSON_Number)
    {
        lane->lane[lane_id].status = temp->valueint;
        LYY_LOGD("lane[%d].status = %d",lane_id,lane->lane[lane_id].status);
    }
    temp = cJSON_GetObjectItem(cJsonData, "startupUnit");
    if(temp != NULL && temp->type == cJSON_Number)
    {
        lane->lane[lane_id].startup_unit = temp->valueint;
        LYY_LOGD("lane[%d].startup_unit = %d",lane_id,lane->lane[lane_id].startup_unit);
    }
    temp = cJSON_GetObjectItem(cJsonData, "enable");
    if(temp != NULL)
    {
        lane->lane[lane_id].enable = (temp->type == cJSON_False ? 0 :1);  //主板协议 0 ：在线  1：离线 所以设置时需要取反
        LYY_LOGD("lane[%d].enable = %d",lane_id,lane->lane[lane_id].enable);
    } 
    temp = cJSON_GetObjectItem(cJsonData, "giftId");
    if(temp != NULL && temp->type == cJSON_Number)
    {
        lane->lane[lane_id].giftId = temp->valueint;
        LYY_LOGD("lane[%d].giftId = %ld",lane_id,lane->lane[lane_id].giftId);
    }
    temp = cJSON_GetObjectItem(cJsonData, "giftName");
    if(temp != NULL && temp->type == cJSON_String)
    {
        memset(lane->lane[lane_id].giftName,0,sizeof(lane->lane[lane_id].giftName));
        strcpy(lane->lane[lane_id].giftName,temp->valuestring);
        LYY_LOGD("lane[%d].giftName = %s",lane_id,lane->lane[lane_id].giftName);
    }
    lane->flag[lane_id] = 1;     //数据线缓存再使能标志位
    cJSON_Delete(cJsonData);        //释放资源

}

/**
 * @description: 货道信息设置
 * @param {*}
 * @return {*}
 */
int property_write_ndj_lane(cJSON* params)
{

    //ndj_add
    PLANE_INFOR_TABLE_DEF lane = ndj_net_lane_infor();
    uint8_t i;
    if((params == NULL) || (params->type != cJSON_Array))
    {
        LYY_LOGD("lane infor error");
        return -1;
    }
    lane->number = cJSON_GetArraySize(params);	//获取数组成员个数 
    lane->number = (lane->number >= NDJ_LANE_MAX ? NDJ_LANE_MAX : lane->number);
    LYY_LOGD("lane->number: %d",lane->number);

    for(i=0;i<lane->number;i++)
    {
        ndj_lane_param_update(lane,params,i);
    }
    vTaskDelay(500 / portTICK_PERIOD_MS);  
    return 1;

}

DEV_FAULT dev_fault_t;

/**
 * @brief Get the dev fault object
 * 
 * @return DEV_FAULT* 
 */
DEV_FAULT *get_dev_fault_para(void)
{
    return &dev_fault_t;
}

/**
 * @brief 
 * 
 */
void dev_fault_init(void)
{
    for(uint8_t i=0;i<FAULT_MSG_MAX;i++)
    {
        dev_fault_t.fault_t[i].errCode = -1;
    }
    
    // lyy_hw_err_msg_read((uint8_t *)&dev_fault_t);
    
    // if(ret != sizeof(DEV_FAULT))
    // {
    //     memset((void *)&dev_fault_t,0xFF,sizeof(DEV_FAULT));
    // }
}

/**
 * @brief Get the fault msg object
 * 
 * @param code 
 * @return char* 
 */
char *get_fault_msg(uint16_t code)
{
    // static char temp[20] = {0};

    for(uint8_t i=0;i<FAULT_MSG_MAX;i++)
    {
        if(code == dev_fault_t.fault_t[i].errCode)
        {
            return dev_fault_t.fault_t[i].errMsg;
        }
    }
    // sprintf(temp,"undefine");
    return "undefine";
}

/**
 * @brief 
 * 故障信息
 * @param params 
 * @return char 
 */
int property_write_fault_list(cJSON* params)
{

    uint32_t ulLen;
    cJSON *data = NULL;
    DEV_FAULT *fault = get_dev_fault_para();
    // 语言
    data = cJSON_GetObjectItem(params, "language");
    if((data != NULL) && (data->type == cJSON_String))
    {
        LYY_LOGD("language: %s", data->valuestring);
        strcpy(fault->language,data->valuestring);
    }

    // 错误信息
    data = cJSON_GetObjectItem(params, "errors");
    if((data != NULL) && (data->type == cJSON_String))
    {

        LYY_LOGD("errors: %s", data->valuestring);
        cJSON *cJsonData =  cJSON_Parse(data->valuestring);
        if(cJsonData == NULL)   
        {
            LYY_LOGD("cJSON_Parse fail");
            return -2;
        }

        fault->num = cJSON_GetArraySize(cJsonData);	//获取数组成员个数 
        LYY_LOGD("fault size %d", fault->num);
        for(uint8_t i=0; i< fault->num; i++)
        {

            //渠道编号
            cJSON *temp = cJSON_GetObjectItem(cJSON_GetArrayItem(cJsonData, i), "errCode");
            if(temp != NULL)
            {
                LYY_LOGD("code[%d] = %d", i, temp->valueint);
                fault->fault_t[i].errCode = temp->valueint;
            }

            temp= cJSON_GetObjectItem(cJSON_GetArrayItem(cJsonData, i), "errMsg");
            if(temp != NULL)
            {
                LYY_LOGD("describe[%d] = %s", i, temp->valuestring);
                ulLen = strlen(temp->valuestring) > 
                            sizeof(fault->fault_t[i].errMsg) ? sizeof(fault->fault_t[i].errMsg) : strlen(temp->valuestring);
                memcpy(fault->fault_t[i].errMsg, temp->valuestring, ulLen);
            }

            temp = cJSON_GetObjectItem(cJSON_GetArrayItem(cJsonData, i), "errFlag");

            if(temp != NULL )
            {
                
                fault->fault_t[i].errFlag = (temp->type == cJSON_True) ? 0x01 :0x00;
                LYY_LOGD("level[%d] = %d", i, fault->fault_t[i].errFlag);
            }

            if(i >= FAULT_MSG_MAX)
                break;
        }
    }

    // lyy_hw_err_msg_write((uint8_t *)fault);
    return 0;
}

/**
 * @brief 
 * 故障信息
 * @param params 
 * @return char 
 */
int property_write_picture_screen(cJSON* params)
{
    (void)params;
  
    tjc_recv_pic_url(params);

    return 0;
}

/**
 * @brief Set vending drop detection enable state.
 */
int property_write_vending_drop_detection(cJSON* params)
{
    uint8_t enable = 0;
    if(params == NULL)
    {
        return -1;
    }
    if(params->type == cJSON_True)
    {
        enable = 1;
    }
    else if(params->type == cJSON_False)
    {
        enable = 0;
    }
    else if(params->type == cJSON_Number)
    {
        enable = (params->valueint != 0) ? 1 : 0;
    }
    else
    {
        LYY_LOGD("vending_drop_detection type error");
        return -1;
    }
    LYY_LOGD("set vending_drop_detection %d", enable);
    yunanshj_dispense_detect_query_report_pending();
    yunanshj_cmd_set_dispense_detect(enable);
    return 1;
}

/**
 * @description: 服务器读取设备波特率
 * @param {*}
 * @return {*}
 */
int property_read_brt_param(cJSON* reply)
{

    char brtBuff[12] = {0x00,};

    if(reply == NULL) 
    {
        return -1;
    }  

    sprintf(brtBuff, "%ld", APP_ulGetParamBaudRate());
    cJSON_AddStringToObject(reply, "uart_brt", brtBuff);

    return 1;
}

void property_post_vending_drop_detection(uint8_t enable)
{
    cJSON* params = cJSON_CreateObject();
    if(params == NULL)
    {
        LYY_LOGE("### vending_drop_detection create fail");
        return;
    }

    if(enable)
    {
        cJSON_AddTrueToObject(params, "vending_drop_detection");
    }
    else
    {
        cJSON_AddFalseToObject(params, "vending_drop_detection");
    }

    LYY_LOGD("post vending_drop_detection %d", enable ? 1 : 0);
    thing_property_post(params);
}


/**
 * @brief 
 * 读取网络信息
 * @param reply 
 * @return int 
 */
int property_read_infor_net(cJSON* reply)
{

    cJSON* cjData = NULL;
    char buff[30] = {0x00,};

    if(reply == NULL) return 0;

    cjData = cJSON_CreateObject();//需要返回到上一层，不需要delete
    if(cjData != NULL)
    {
        cJSON_AddStringToObject(cjData, "net_type", "wifi");

        HAL_vGetDevMac(buff);
        cJSON_AddStringToObject(cjData, "imei",   buff);

        cJSON_AddStringToObject(cjData, "iccid",  "RTL8711");   


        cJSON_AddItemToObject(reply, "infor_net", cjData);
    }

    return 1;

}

/**
 * @brief 
 * 读取脉冲参数
 * @param reply 
 * @return char 
 */
int property_read_pulse_para(cJSON* reply)
{

    APP_PULSE_T   *pxPulseInfo  = APP_pxlGetParamePulse();

    if(reply == NULL)   return -1;

    cJSON* cjData = NULL;

    cjData = cJSON_CreateObject();//需要返回到上一层，不需要delete
    if(cjData != NULL)
    {

        cJSON_AddNumberToObject(cjData, "gap",      pxPulseInfo->ulPulseSpace);
        cJSON_AddNumberToObject(cjData, "width",    pxPulseInfo->ulPulseWidth);
        cJSON_AddStringToObject(cjData, "mode",     !APP_ulGetMachtineLvl() ? ("NO"):("NC"));
        cJSON_AddStringToObject(cjData, "output_type", ((START_TYPE_LEVEL == APP_pxlGetParamePulse()->ulPulsetype) ?("level"):("pulse")));
        cJSON_AddItemToObject(reply, "pulse_para", cjData);

    }

    return 1;

}

/**
 * @brief 
 * 登录信息更新
 * @param login 
 * @param flag 
 * @param msg 
 */
void device_login_infor_update(uint16_t login,uint8_t flag,char *msg)
{

    dev_login_infor_t.login  = login;
    dev_login_infor_t.result = flag;
    memset(dev_login_infor_t.msg,0,sizeof(dev_login_infor_t.msg));
    strcpy(dev_login_infor_t.msg,msg);

}

/**
 * @brief 
 * 读取登录标识
 * @param reply 
 * @return char 
 */
int property_read_login_flag(cJSON* reply)
{

    if(reply == NULL)   return -1;
    cJSON* login = NULL;
    login = cJSON_CreateObject();//需要返回到上一层，不需要delete
    if(login == NULL) return -2;
    if(dev_login_infor_t.result == 0x00) return -3;
    login_flag_packet(login,dev_login_infor_t.login,dev_login_infor_t.result,dev_login_infor_t.msg);
    cJSON_AddItemToObject(reply, "login_flag", login);

    return 1;
}


/**
 * @brief 
 * 上报登录标识数据包
 * @param login 
 * @param login_flag 
 * @param result 
 * @param message 
 */
void login_flag_packet(cJSON *login,uint16_t login_flag,uint8_t result,char *message)
{

    if(login != NULL)
    {        
        cJSON_AddNumberToObject(login, "value", login_flag);
        if(result == 0x01) cJSON_AddTrueToObject(login,"success");
        else cJSON_AddFalseToObject(login,"success");
        cJSON_AddStringToObject(login, "msg", message); 
    }    

}

/**
 * @brief 
 * 
 * @param login_flag 
 * @param result 
 * @param message 
 */
void property_post_login_flag(uint16_t login_flag,uint8_t result,char *message)
{

    cJSON* login = NULL;
    login = cJSON_CreateObject();
    cJSON *data = cJSON_CreateObject();
    if(data==NULL)
    { 
        cJSON_Delete(login);
        return;
    }
    
    login_flag_packet(data,login_flag,result,message);
    cJSON_AddItemToObject(login, "login_flag", data);
    thing_property_post(login);

}


/**
 * @brief 
 * 
 * @param sn 
 * @param soft_ver 
 * @param hard_ver 
 * @param sdk_ver 
 */
void packet_device_infor(cJSON *packet,char *sn,char *soft_ver,char *hard_ver,char *sdk_ver)
{
    if(packet != NULL)
    {       
        //devid-设备唯一码
        cJSON_AddStringToObject(packet, "devid", sn);
        //软件版本
        cJSON_AddStringToObject(packet, "soft_ver",soft_ver);
        //硬件版本
        cJSON_AddStringToObject(packet, "hard_ver",hard_ver);
        //sdk版本
        cJSON_AddStringToObject(packet, "sdk_ver", sdk_ver);    
        //统一固件版本
        #define FIRMWARE_INFOR_STD      "V1"
        cJSON_AddStringToObject(packet, "firmware_infor", FIRMWARE_INFOR_STD);    
    }      
}

/**
 * @brief 
 * 
 * @param reply 
 * @return int 
 */
int property_read_dev_infor(cJSON* reply)
{

    if(reply == NULL)   return -1;

    cJSON* data = cJSON_CreateObject();//需要返回到上一层，不需要delete
    if(data == NULL) return -2;


    packet_device_infor(data,   
                        APP_pcGetParamDevId(), 
                        CFG_APP_VERSION, 
                        CFG_HARD_VERSION , 
                        CFG_SDK_VERSION);

    cJSON_AddItemToObject(reply, "dev_infor", data);

    return 1;
}



/**
 * @brief 
 * 设备信息上报
 */
void property_post_dev_infor(void)//
{

    cJSON* data         = cJSON_CreateObject();

    if(NULL == data) {

        return ;
    }

    cJSON* dev_infor    = cJSON_CreateObject();

    if(NULL == dev_infor) {

        cJSON_Delete(data);
        return ;
    }

    packet_device_infor(data,                        
                        APP_pcGetParamDevId(), 
                        CFG_APP_VERSION, 
                        CFG_HARD_VERSION , 
                        CFG_SDK_VERSION);

    cJSON_AddItemToObject(dev_infor, "dev_infor", data);
    thing_property_post(dev_infor);        
  

}

/**
 * @description: 网络类型上报
 * {
        "network_type": "NET5G"
	}
 * @param {*}
 * @return {*}
 */
void property_post_net_infor(void)
{

    cJSON* data         = NULL;
    cJSON* infor_net    = NULL;
    char buff[32]       = {0x00,};

    data = cJSON_CreateObject();
    if(!data)
    {
        LYY_LOGE("### data create object fail");
        return ;
    }

    infor_net = cJSON_CreateObject();
    if(!infor_net)
    {
        LYY_LOGE("### infor_net create object fail");
        cJSON_Delete(data);
        return ;
    } 

    //network_type-网络类型
    cJSON_AddStringToObject(data, "net_type",       "wifi");

    HAL_vGetDevMac(buff);
    cJSON_AddStringToObject(data, "imei",           buff);

    cJSON_AddStringToObject(data, "iccid",          "RTL8711");   

    cJSON_AddItemToObject(infor_net, "infor_net",   data);

    thing_property_post(infor_net);

}

/**
 * @description: 串口波特率
 * @param {*}
 * @return {*}
 */
void property_post_uart_brt_infor(void)
{

    char brtBuff[10] = {0x00,};
    cJSON* uart_brt = cJSON_CreateObject();

    if(uart_brt != NULL)
    {      
        sprintf(brtBuff, "%ld", APP_ulGetParamBaudRate());
        cJSON_AddStringToObject(uart_brt, "uart_brt", brtBuff);
        thing_property_post(uart_brt);        
    }  

}

/**
 * @brief 
 * 上报网络信号
 */
void property_post_csq_level(void)
{

    cJSON *params = cJSON_CreateObject();
    if(params == NULL)
    {
        LYY_LOGE("### params create fail");
        return ;
    }
       
    cJSON *network_level = cJSON_CreateObject();
    if(params == NULL)
    {
        cJSON_Delete(params);
        LYY_LOGE("### network_level create fail");
        return ;
    } 

    //network_type-网络类型
    cJSON_AddNumberToObject(network_level,  "csq",              HAL_lGetWifiRssiToCsq());
    cJSON_AddStringToObject(network_level,  "network_type",     "WIFI");
    cJSON_AddItemToObject(params,           "network_level",    network_level);

    thing_property_post(params);
    
}

/**
 * @brief 
 * lbs cell 上报
 */
void property_post_lbs_cell(void *para)
{
    
    (void)para;

    cJSON* data = NULL;
    
    data = cJSON_CreateObject();

    if(data != NULL)
    {

        cJSON_AddNumberToObject(data, "rssi", 31);
        cJSON_AddNumberToObject(data, "rssi", 31);

        //devid-设备唯一码
        cJSON_AddStringToObject(data, "network_type", "WIFI");

        cJSON* network_level = NULL;
        network_level = cJSON_CreateObject();
        if(!network_level) {
            
            cJSON_Delete(data);
            return ;
        }
        cJSON_AddItemToObject(network_level, "network_level", data);
        thing_property_post(network_level);

    }   

}

