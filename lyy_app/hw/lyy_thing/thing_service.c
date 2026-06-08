/*
 * @author: e.he
 * @Date: 2024-06-20 17:19:36
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-08 18:31:15
 * @Description: 
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>


#include "utils.h"
#include "cJSON.h"

#include "HAL_Log.h"
#include "HAL_Uart.h"
#include "HAL_System.h"

#include "APP_Param.h"
#include "APP_Pluse.h"
#include "APP_Detect.h"

#include "thing_iot.h"
#include "thing_init.h"
#include "thing_event.h"
#include "thing_service.h"
#include "thing_property.h"


#include "lyy_tjc.h"
#include "lyy_ndj.h"
#include "lyy_para.h"
#include "lyy_start.h"
#include "lyy_dfssj.h"

int thing_service_pay_apply_ndj(cJSON* params, char *id);
int thing_service_pay_start_ndj(cJSON* params, char *id);
int thing_service_payment_code(cJSON* params, char *id);
int thing_service_qrcode_dynamic(cJSON* params, char *id);
int thing_service_pay_result(cJSON* params, char *id);
int thing_service_lane_infor_get(cJSON* params, char *id);
int thing_service_packages_set_ndj(cJSON* params, char *id);
int thing_service_lane_infor_set(cJSON* params, char *id);
int thing_service_restart(cJSON* params, char *id);
int thing_service_equipment_binding(cJSON *params, char *id);
int thing_service_equipment_unbinding(cJSON *params, char *id);

int thing_service_pay_apply(cJSON* params, char *id);
int thing_service_pay_start(cJSON* params, char *id);
int thing_service_pay_apply_dbj(cJSON* params, char *id);
int thing_service_pay_start_dbj(cJSON* params, char *id);
int thing_service_pay_apply_wwj(cJSON* params, char *id);
int thing_service_pay_start_wwj(cJSON* params, char *id);
int thing_service_pay_start_amy(cJSON* params, char *id);
int thing_service_pay_apply_dfssj(cJSON *params, char *id);
int thing_service_pay_start_dfssj(cJSON *params, char *id);
/**
 * @brief
 * 服务
 */
const SERVICE_METHOD_DEF moethod_service[] =
    {
        {"pay_apply", thing_service_pay_apply},
        {"pay_start", thing_service_pay_start},
        {"out_coins", thing_service_pay_start_dbj},
        {"pay_qr_code", thing_service_payment_code},                     // 支付二维码下发
        {"qrcode_post", thing_service_qrcode_dynamic},                   // 支付动态二维码下发
        {"pay_notice", thing_service_pay_result},                        // 支付结果下发
        {"device_position_info", thing_service_lane_infor_get},          // 货道信息查询
        {"update_position_pay_package", thing_service_packages_set_ndj}, // 更新套餐
        {"pay_package", thing_service_packages_set_ndj},                 // 更新套餐
        {"update_position_info", thing_service_lane_infor_set},          // 更新货道
        {"reboot", thing_service_restart},
        {"event_notification", NULL},
        {"equipment_binding", thing_service_equipment_binding},     // 设备绑定通知
        {"equipment_unbinding", thing_service_equipment_unbinding}, // 设备解绑通知
};

char pay_start_type[64] = {0};
/**
 * @brief Get the apply start result type object
 * 获取申请/启动结果类型
 * @param code 
 * @return char* 
 */
char *get_apply_start_result_type(uint16_t code)
{
    static char  rsl[30]={0};
    memset(rsl,0,sizeof(rsl));
    if(code==PAY_START_RESULT_FAIL) strcpy(rsl,"fail");
    else if(code == PAY_START_RESULT_SUCCESS) strcpy(rsl,"success");
    else if(code == PAY_START_RESULT_FAULT) strcpy(rsl,"fault");
    else if(code == PAY_START_RESULT_OFFLINE) strcpy(rsl,"offline");
    else if(code == PAY_START_RESULT_STARTING) strcpy(rsl,"starting");
    else if(code == PAY_START_RESULT_TIMEOUT) strcpy(rsl,"timeout");
    else if(code == PAY_START_RESULT_SUCCESS_DBJ) strcpy(rsl,"start_normally");
    else if(code == PAY_START_RESULT_FAIL_DBJ) strcpy(rsl,"startup_failed");
    else  strcpy(rsl,"other");
    return  rsl;  
}
/**
 * @brief 
 * 服务
 * @param params 
 * @param method 
 * @param id 
 * @return int 
 * <0 失败
 * 其他成功
 */
int thing_service_parse(cJSON* params,char *method,char *id)
{
    SERVICE_METHOD_DEF *service = NULL;
    int ret = -1;

    for(uint8_t i=0; i<(sizeof(moethod_service)/sizeof(SERVICE_METHOD_DEF)); i++)
    {
        service = (SERVICE_METHOD_DEF *)&moethod_service[i];
        if(NULL != strstr(method, service->method))
        {
            if(service->parse != NULL)
                ret = service->parse(params ,id);
            LYY_LOGD("method: %s ;ret = %d", method, ret);
            return ret;
        }
    }
    return ret;
}
/**
 * @brief 
 * 支付申请
 * @param params 
 * @param id 
 * @return int 
 */
int thing_service_pay_apply(cJSON* params, char *id)
{
    switch(get_dev_type_code())
    {
        case DEVICE_TYPE_UART_NDJ:thing_service_pay_apply_ndj(params,id);break;
        case DEVICE_TYPE_UART_DFSSJ:thing_service_pay_apply_dfssj(params,id);break;
        case DEVICE_TYPE_UART_DBJ:thing_service_pay_apply_dbj(params,id);break;
        case DEVICE_TYPE_UART_THXCJ:thing_service_pay_apply_wwj(params,id);break;
        default:thing_service_pay_apply_wwj(params,id);break;
    }
    return 1;
}

/**
 * @brief 
 * 
 * @param result 
 * @param number 
 * @param order 
 */
void event_post_start_result_pulse(uint16_t result,uint32_t number,char *order)
{

    (void)result;

    cJSON* value = NULL;
    value = cJSON_CreateObject();
    if(value != NULL)
    {
        cJSON_AddStringToObject(value, "result","success");
        cJSON_AddNumberToObject(value, "number", number);
        cJSON_AddStringToObject(value, "order", order);
        thing_event_post(value, "start_result"); 
    }
}

/**
 * @brief 
 * 启动 --- 脉冲
 * @param params 
 * @param id 
 * @return lyy_int8_t 
 */
int thing_service_pay_start_pulse(cJSON* params, char *id)
{

    cJSON *temp = NULL;
    uint32_t coins=0; 
    char *order = NULL;
    temp = cJSON_GetObjectItem(params, "number");
    if((temp != NULL) && (temp->type == cJSON_Number)) 
    {
        LYY_LOGD("number %ld", temp->valueint);
        coins = temp->valueint;
    }
    
    temp = cJSON_GetObjectItem(params, "order");
    if((temp == NULL) || (temp->type != cJSON_String)) 
    {
        return -1;
    }
        
    order = temp->valuestring;
    LYY_LOGD("order %s", temp->valuestring);

    api_std_start_entryQue(START_TYPE_PAY_START,coins,order,0,0,id);

    thing_service_reply_pay_wwj(order,1,id);

    vTaskDelay(300 / portTICK_PERIOD_MS);

    event_post_start_result_pulse(0x01, coins, order);

    return 1;

}

/**
 * @brief 
 * 
 * @param params 
 * @param id 
 * @return int 
 */
int thing_service_pay_start(cJSON* params, char *id)
{
    switch(get_dev_type_code())
    {
        case DEVICE_TYPE_LEVEL:
        case DEVICE_TYPE_PULSE:
        {
           thing_service_pay_start_pulse(params,id); 
        }break;
        case DEVICE_TYPE_UART_QHXYJ:
        case DEVICE_TYPE_UART_LJXYJ:
        case DEVICE_TYPE_UART_QHHGJ:
        case DEVICE_TYPE_UART_LJHGJ:
        case DEVICE_TYPE_UART_SKYWORTH_GTXYJ:
        case DEVICE_TYPE_UART_SKYWORTH_GTHGJ:
        case DEVICE_TYPE_UART_SKYWORTH_GTXHJ:
        case DEVICE_TYPE_UART_MSHGJ:
        case DEVICE_TYPE_UART_TCLXYJ:
            thing_service_pay_start_qhxyj(params, id);
            break;
        case DEVICE_TYPE_UART_DFSSJ:
            thing_service_pay_start_dfssj(params, id);
            break; // 
        case DEVICE_TYPE_UART_WWJ:thing_service_pay_start_wwj(params,id);break;
        case DEVICE_TYPE_UART_NDJ:thing_service_pay_start_ndj(params,id);break;
        case DEVICE_TYPE_UART_DBJ:thing_service_pay_start_dbj(params,id);break;
        case DEVICE_TYPE_UART_AMY:thing_service_pay_start_amy(params,id);break;
        case DEVICE_TYPE_UART_THXCJ:thing_service_pay_start_amy(params,id);break;
        default:return -1;break;
    }
    return 1;
}

/***********************************ndj*************************************/
/**
 * @brief 
 * 支付申请
 * @param params 
 * @param id 
 * @return char 
 */
int thing_service_pay_apply_ndj(cJSON* params, char *id)
{
    //ndj_add
    int x=0,y=0;
    cJSON *position = NULL;
    cJSON *order = cJSON_GetObjectItem(params, "order");
    if((order == NULL) || (order->type != cJSON_String)) return -1;
    LYY_LOGD("pay id %s", order->valuestring);
    position = cJSON_GetObjectItem(params, "position_x");
    if((position == NULL) || (position->type != cJSON_Number)) return -1;
    if(position->valueint <= 0)return -1;
    x = position->valueint - 1;
    LYY_LOGD("position_x %d", x);
    position = cJSON_GetObjectItem(params, "position_y");
    if((position != NULL) && (position->type == cJSON_Number)) y = position->valueint;

    return api_std_start_entryQue(START_TYPE_PAY_APPLY,0,order->valuestring ,x,y,id); //支付申请入队

}
/***********************************DFSSJ*************************************/
/**
 * @brief
 * 支付申请
 * @param params
 * @param id
 * @return char
 */
int thing_service_pay_apply_dfssj(cJSON *params, char *id)
{
    // ndj_add
    int x = 0, y = 0;
    cJSON *position = NULL;
    cJSON *order = cJSON_GetObjectItem(params, "order");
    if ((order == NULL) || (order->type != cJSON_String))
        return -1;
    LYY_LOGD("pay id %s", order->valuestring);
    position = cJSON_GetObjectItem(params, "position_x");
    if ((position == NULL) || (position->type != cJSON_Number))
        return -1;
    if (position->valueint <= 0)
        return -1;
    x = position->valueint - 1;
    LYY_LOGD("position_x %d", x);
    position = cJSON_GetObjectItem(params, "position_y");
    if ((position != NULL) && (position->type == cJSON_Number))
        y = position->valueint;

    return api_std_start_entryQue(START_TYPE_PAY_APPLY, 0, order->valuestring, x, y, id); // 支付申请入队
}
/**
 * @brief 
 * 支付申请应答
 * @param id 
 * @param pay_id 
 * @param pay_status 
 * @param pack_id 
 */
void thing_service_reply_pay_apply_ndj(uint8_t id,char *order,uint16_t result,char *pack_id)
{
    cJSON* data = NULL;
    data = cJSON_CreateObject();
    if(data != NULL)
    {
        cJSON_AddStringToObject(data, "order", order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        cJSON_AddNumberToObject(data, "position_x", id);
        cJSON_AddNumberToObject(data, "position_y", 0);
        thing_service_invoke_reply(data, "success", pack_id, "pay_apply");
    }
}

//---------------------------qhxiyiji----------------------------------------------------
/**
 * @brief
 * 支付申请---乔禾洗衣机
 * @param params
 * @param id
 * @return lyy_int8_t
 */
int thing_service_pay_apply_qhxyj(cJSON *params, char *id)
{
    cJSON *order = cJSON_GetObjectItem(params, "order");
    if ((order == NULL) || (order->type != cJSON_String))
        return -1;
    LYY_LOGD("thing_service_pay_apply_qhxyj >>>>>>>>\r\n");
    return api_std_start_entryQue(START_TYPE_PAY_APPLY, 0, order->valuestring, 0, 0, id); // 支付申请入队
}

/**
 * @brief
 * 支付申请应答---乔禾洗衣机
 * @param id
 * @param pay_id
 * @param pay_status
 * @param pack_id
 */
void thing_service_reply_pay_apply_qhxyj(char *order, uint16_t result, char *pack_id)
{
    cJSON *data = NULL;
    data = cJSON_CreateObject();
    if (data != NULL)
    {
        cJSON_AddStringToObject(data, "order", order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        thing_service_invoke_reply(data, "success", pack_id, "pay_apply");
    }
}
/**
 * @brief
 * 支付申请---乔禾烘干机
 * @param params
 * @param id
 * @return lyy_int8_t
 */
int thing_service_pay_apply_qhhgj(cJSON *params, char *id)
{
    cJSON *order = cJSON_GetObjectItem(params, "order");
    if ((order == NULL) || (order->type != cJSON_String))
        return -1;
    return api_std_start_entryQue(START_TYPE_PAY_APPLY, 0, order->valuestring, 0, 0, id); // 支付申请入队
}

/**
 * @brief
 * 支付申请应答---乔禾烘干机
 * @param id
 * @param pay_id
 * @param pay_status
 * @param pack_id
 */
void thing_service_reply_pay_apply_qhhgj(char *order, uint16_t result, char *pack_id)
{
    cJSON *data = NULL;
    data = cJSON_CreateObject();
    if (data != NULL)
    {
        cJSON_AddStringToObject(data, "order", order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        thing_service_invoke_reply(data, "success", pack_id, "pay_apply");
    }
}
/**
 * @brief
 * 启动 -- 乔禾洗衣机
 * @param params
 * @param id
 * @return lyy_int8_t
 */
int thing_service_pay_start_qhxyj(cJSON *params, char *id)
{
    cJSON *temp = NULL;
    int32_t mode;
    char *order = NULL;

    temp = cJSON_GetObjectItem(params, "order");
    if ((temp == NULL) || (temp->type != cJSON_String))
        return -1;
    order = temp->valuestring;
    LYY_LOGD("order %s", temp->valuestring);

    temp = cJSON_GetObjectItem(params, "mode");
    LYY_LOGD("mode  type:%d  value:%s", temp->type, temp->valuestring);
    if ((temp == NULL) || (temp->type != cJSON_String))
        return -1;
    // mode = temp->valuestring[0]-'0';
    mode = atoi(temp->valuestring);
    LYY_LOGD("mode  %d", mode);

    return api_std_start_entryQue(START_TYPE_PAY_START, mode, order, 0, 0, id);
}

/**
 * @brief
 * 启动应答 -- 乔禾洗衣机
 * @param order
 * @param result
 * @param pack_id
 */
void thing_service_reply_pay_start_qhxyj(char *order, uint16_t result, char *pack_id)
{
    cJSON *data = NULL;
    data = cJSON_CreateObject();
    if (data != NULL)
    {
        cJSON_AddStringToObject(data, "order", order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        thing_service_invoke_reply(data, "success", pack_id, "pay_start");
    }
}

/**
 * @brief
 * 启动 -- 乔禾烘干机
 * @param params
 * @param id
 * @return lyy_int8_t
 */
int thing_service_pay_start_qhhgj(cJSON *params, char *id)
{
    cJSON *temp = NULL;
    int32_t mode;
    char *order = NULL;

    temp = cJSON_GetObjectItem(params, "order");
    if ((temp == NULL) || (temp->type != cJSON_String))
        return -1;
    order = temp->valuestring;
    LYY_LOGD("order %s", temp->valuestring);

    temp = cJSON_GetObjectItem(params, "mode");
    LYY_LOGD("mode  type:%d  value:%s", temp->type, temp->valuestring);
    if ((temp == NULL) || (temp->type != cJSON_String))
        return -1;
    mode = temp->valuestring[0] - '0';
    LYY_LOGD("mode  %d", mode);

    return api_std_start_entryQue(START_TYPE_PAY_START, mode, order, 0, 0, id);
}

/**
 * @brief
 * 启动应答 -- 乔禾烘干机
 * @param order
 * @param result
 * @param pack_id
 */
void thing_service_reply_pay_start_qhhgj(char *order, uint16_t result, char *pack_id)
{
    cJSON *data = NULL;
    data = cJSON_CreateObject();
    if (data != NULL)
    {
        cJSON_AddStringToObject(data, "order", order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        thing_service_invoke_reply(data, "success", pack_id, "pay_start");
    }
}
/**
 * @brief 
 * 启动应答
 * @param id 
 * @param pay_id 
 * @param pay_result 
 * @param pack_id 
 */
void thing_service_reply_pay_start_ndj(uint8_t id,char *order,uint16_t result,char *pack_id)
{
    cJSON* data = NULL;
    data = cJSON_CreateObject();  
    if(data != NULL)
    {
        cJSON_AddStringToObject(data, "order", order);
        cJSON_AddStringToObject(data, "startup_type", pay_start_type);  //待补充
        cJSON_AddNumberToObject(data, "coins", result);
        cJSON_AddNumberToObject(data, "position_x", id);
        cJSON_AddNumberToObject(data, "position_y", 0);
        thing_service_invoke_reply(data, "success", pack_id, "pay_start");
    }
}

/**
 * @brief
 * 启动应答
 * @param id
 * @param pay_id
 * @param pay_result
 * @param pack_id
 */
void thing_service_reply_pay_start_dfssj(uint8_t id, char *order, uint16_t result, char *pack_id)
{
    cJSON *data = NULL;
    data = cJSON_CreateObject();
    if (data != NULL)
    {
        cJSON_AddStringToObject(data, "order", order);
        cJSON_AddStringToObject(data, "startup_type", pay_start_type); // 待补充
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        cJSON_AddNumberToObject(data, "position_x", id);
        cJSON_AddNumberToObject(data, "position_y", 0);
        thing_service_invoke_reply(data, "success", pack_id, "pay_start");
    }
}
/**
 * @brief 
 * 启动
 * @param params 
 * @param id 
 * @return char 
 */
int thing_service_pay_start_ndj(cJSON* params, char *id)
{
    //ndj_add
    cJSON *temp = NULL;
    int32_t coins; 
    char *order = NULL;
    int x=0,y=0;
    temp = cJSON_GetObjectItem(params, "pay_amount");
    if((temp != NULL) && (temp->type == cJSON_Number)) LYY_LOGD("money %f", temp->valuedouble);
    
    temp = cJSON_GetObjectItem(params, "order");
    if((temp == NULL) || (temp->type != cJSON_String)) return -1;
    order = temp->valuestring;
    LYY_LOGD("order %s", temp->valuestring);

    temp = cJSON_GetObjectItem(params, "coins");
    if((temp == NULL) || (temp->type != cJSON_Number))  return -1;
    coins = temp->valueint;
    LYY_LOGD("coins  %d", coins);

    temp = cJSON_GetObjectItem(params, "position_x");
    if((temp == NULL) || (temp->type != cJSON_Number))  return -1;  
    if(temp->valueint <= 0)return -1;
    x = temp->valueint - 1;
    LYY_LOGD("x  %d", x);

    temp = cJSON_GetObjectItem(params, "position_y");
    if((temp != NULL) && (temp->type == cJSON_Number))  y = temp->valueint;    
    LYY_LOGD("y  %d", y);

    memset(pay_start_type,0,sizeof(pay_start_type));
    temp = cJSON_GetObjectItem(params, "startup_type");
    if((temp != NULL) && (temp->type == cJSON_String))  strcpy(pay_start_type,temp->valuestring);    
    LYY_LOGD("pay_start_type  %s", pay_start_type);

    return api_std_start_entryQue(START_TYPE_PAY_START,coins,order,x,y,id);

}

/**
 * @brief
 * 启动
 * @param params
 * @param id
 * @return char
 */
int thing_service_pay_start_dfssj(cJSON *params, char *id)
{
    // ndj_add
    cJSON *temp = NULL;
    int16_t number;
    char *order = NULL;
    int x = 0, y = 0;

    temp = cJSON_GetObjectItem(params, "order");
    if ((temp == NULL) || (temp->type != cJSON_String))
        return -1;
    order = temp->valuestring;
    LYY_LOGD("order %s", temp->valuestring);

    temp = cJSON_GetObjectItem(params, "number");
    if ((temp == NULL) || (temp->type != cJSON_Number))
        return -1;
    number = temp->valueint;
    LYY_LOGD("number  %d", number);

    temp = cJSON_GetObjectItem(params, "position_x");
    if ((temp == NULL) || (temp->type != cJSON_Number))
        return -1;
    if (temp->valueint <= 0)
        return -1;
    x = temp->valueint - 1;
    LYY_LOGD("x  %d", x);

    temp = cJSON_GetObjectItem(params, "position_y");
    if ((temp != NULL) && (temp->type == cJSON_Number))
        y = temp->valueint;
    LYY_LOGD("y  %d", y);

    memset(pay_start_type, 0, sizeof(pay_start_type));
    temp = cJSON_GetObjectItem(params, "startup_type");
    if ((temp != NULL) && (temp->type == cJSON_String))
        strcpy(pay_start_type, temp->valuestring);
    LYY_LOGD("pay_start_type  %s", pay_start_type);

    return api_std_start_entryQue(START_TYPE_PAY_START, number, order, x, y, id);
}
/**
 * @brief 
 * 获取货道信息
 * @param params 
 * @param id 
 * @return int 
 */
int thing_service_lane_infor_get(cJSON* params, char *id)
{
    //ndj_add
    int32_t x=0,y=0;
    cJSON* result = cJSON_GetObjectItem(params, "position_x");
    if(result != NULL&&result->type == cJSON_Number) //启动订单号
    {
        LYY_LOGD("position_x: %d", result->valueint);
        x = result->valueint;
    }
    else
    {
        LYY_LOGD("no lane number");
        return 0;
    }
    result = cJSON_GetObjectItem(params, "position_y");
    if(result != NULL&&result->type == cJSON_Number) //启动订单号
    {
        LYY_LOGD("position_y: %d", result->valueint);
        y = result->valueint;
    }
    else
    {
        LYY_LOGD("no lane number");
    }
    return api_std_start_entryQue(START_TYPE_NDJ_LANE_INFOR_GET,0,NULL,x,y,id);
}

/**
 * @brief 
 * 获取货道信息应答
 * @param lane_id 
 * @param status 
 * @param coins 
 * @param time 
 * @param id 
 */
void thing_service_reply_ndj_lane_get(uint8_t lane_id,uint8_t status,uint8_t coins,uint8_t time,char *id)
{
    cJSON* ack = NULL;
    ack = cJSON_CreateObject();
    if(ack != NULL)
    {
        cJSON_AddNumberToObject(ack,"position_x",lane_id);
        cJSON_AddNumberToObject(ack,"position_y",0);
        cJSON_AddNumberToObject(ack,"status",status);
        cJSON_AddNumberToObject(ack,"play_time",time);
        cJSON_AddNumberToObject(ack,"startup_unit",coins);
        thing_service_invoke_reply(ack, "success", id, "device_position_info"); //应答
    }    
}
/**
 * @brief 
 * 设置货到应答
 * @param result 
 * @param id 
 */
void thing_service_reply_ndj_lane_set(uint8_t result,char *id)
{
    cJSON* ack = NULL;
    ack = cJSON_CreateObject();
    if(ack != NULL)
    {
        if(result == 0x01)  cJSON_AddTrueToObject(ack,"result");
        else cJSON_AddFalseToObject(ack,"result");
        thing_service_invoke_reply(ack, "success", id, "update_position_info"); //应答
    }    
}

/**
 * @brief 
 * 更新套餐
 * @param params 
 * @param id 
 * @return int 
 */
int thing_service_packages_set_std(cJSON* params, char *id)
{
    uint8_t  i = 0;
    uint8_t cnt = 0;

    PAY_PACKAGES_DEF *para = get_pay_packages();
    cJSON *data = params;

    cJSON *cJsonData = NULL;
    cJSON *sub_package = NULL;

    if(data->type == cJSON_String)
    {
        LYY_LOGD("pay_package %s", data->valuestring);
    }
    cJSON *pack = cJSON_GetObjectItem(data, "pay_package");
    {
        LYY_LOGD("package set format is error");
        return -1;
    }
    cJsonData = cJSON_Parse(pack->valuestring);
    if(cJsonData == NULL)   
    {
        LYY_LOGD("cJSON_Parse fail");
        return -2;
    }
    //货币 名称
    data = cJSON_GetObjectItem(cJsonData, "currencyCode");
    if(data != NULL)
    {
        memset(para->currency_name, 0x00, sizeof(para->currency_name));
        LYY_LOGD("package currencyCode %s", data->valuestring);
        strcpy(para->currency_name, data->valuestring);
    }

    // 货币 符号
    data = cJSON_GetObjectItem(cJsonData, "currencySymbol");
    if(data != NULL)
    {
        memset(para->currency_symbol, 0x00, sizeof(para->currency_symbol));
        LYY_LOGD("package currencySymbol %s", data->valuestring);
        strcpy(para->currency_symbol, data->valuestring);
    }

    data = cJSON_GetObjectItem(cJsonData, "consumptionPackage");
    if(data == NULL)
    {
        cJSON_Delete(cJsonData);//清除结构体
        return -3;
    }

    cnt = cJSON_GetArraySize(data);	//获取数组成员个数  
    LYY_LOGD("pay_package number %d", cnt);
    if(cnt == 0)//无套餐
    {
        LYY_LOGD("no lane no package");
        return -4; 
    } 
    for(i=0; i < cnt; i++)
    {
        sub_package = cJSON_GetArrayItem(data, i);  
        if(sub_package == NULL) continue;
        pay_package_update(&(para->pkg[i]),sub_package); 
    }
    if(cJsonData != NULL) cJSON_Delete(cJsonData);    //清除结构体

    vTaskDelay(500 / portTICK_PERIOD_MS);
    

    //todo..
    lyy_tjc_page_package(); //显示套餐


    cJSON* ack = NULL;
    ack = cJSON_CreateObject();
    if(ack != NULL)
    {
        cJSON_AddTrueToObject(ack,"result");
        thing_service_invoke_reply(ack, "success", id, "update_position_pay_package"); //应答
    }

    return 1;
}
/**
 * @description: 配置套餐
 * @param {*}
 * @return {*}
 */
int thing_service_packages_set_ndj(cJSON* params, char *id)
{
    //ndj_add
    uint8_t  i = 0;
    uint8_t j=0,cnt = 0,sub_cnt = 0;
    uint8_t lane_id;
    PAY_PACKAGES_NDJ_DEF *para = get_pay_ndj_packages();
    cJSON *data = params;
    cJSON *lane_data = NULL;
    cJSON *package = NULL;
    cJSON *cJsonData = NULL;
    cJSON *sub_package = NULL;
    cJSON *package2 = NULL;
    if(data->type == cJSON_String)
    {
        LYY_LOGD("ndj_pay_package %s", data->valuestring);
    }
    cJSON *pack = cJSON_GetObjectItem(data, "position_pay_package");
    if(pack == NULL)
    {
        LYY_LOGD("package set format is error");
        return -1;
    }
    cJsonData = cJSON_Parse(pack->valuestring);
    if(cJsonData == NULL)   
    {
        LYY_LOGD("cJSON_Parse fail");
        return -2;
    }
    //货币 名称
    data = cJSON_GetObjectItem(cJsonData, "currencyCode");
    if(data != NULL)
    {
        memset(para->currency_name, 0x00, sizeof(para->currency_name));
        LYY_LOGD("package currencyCode %s", data->valuestring);
        strcpy(para->currency_name, data->valuestring);
    }

    // 货币 符号
    data = cJSON_GetObjectItem(cJsonData, "currencySymbol");
    if(data != NULL)
    {
        memset(para->currency_symbol, 0x00, sizeof(para->currency_symbol));
        LYY_LOGD("package currencySymbol %s", data->valuestring);
        strcpy(para->currency_symbol, data->valuestring);
    }

    data = cJSON_GetObjectItem(cJsonData, "positionList");
    if(data == NULL)
    {
        cJSON_Delete(cJsonData);//清除结构体
        return -3;
    }

    cnt = cJSON_GetArraySize(data);	//获取数组成员个数  //更改货道个数
    LYY_LOGD("pay_package number %d", cnt);
    if(cnt == 0)  //无套餐
    {
        LYY_LOGD("no lane no package");
        return -4; 
    } 
    for(i=0; i < cnt; i++)
    {
        sub_package = cJSON_GetArrayItem(data, i);  
        if(sub_package == NULL)
            continue;
        lane_data = cJSON_GetObjectItem(sub_package, "positionX");
        if(lane_data != NULL && lane_data->type == cJSON_Number && (lane_data->valueint >= 1))
        {   
            lane_id = lane_data->valueint - 1; // 从0开始
            if(lane_id >= NDJ_LANE_MAX) continue ; // 仓位ID过大
            para->lane[lane_id].x = lane_data->valueint;
            LYY_LOGD("lane[%d].x=%d", lane_id, lane_data->valueint);
        }
        else continue;  //没有仓位ID跳过后面部分


        lane_data = cJSON_GetObjectItem(sub_package, "positionY");
        if(lane_data != NULL && lane_data->type == cJSON_Number)
        {
            LYY_LOGD("lane[%d].y=%d", lane_id, lane_data->valueint);
            para->lane[lane_id].y = lane_data->valueint;
        }

        package = cJSON_GetObjectItem(sub_package, "consumptionPackage");
        sub_cnt = cJSON_GetArraySize(package);	//获取数组成员个数 
        if(sub_cnt == 0) //无套餐
        {
            // ndj_write_pay_para();
            LYY_LOGD("lane[%d] :no package",lane_id);   
        }

        for(j=0;j<sub_cnt;j++)
        {
            package2 = cJSON_GetArrayItem(package, j); 
            if(package2 == NULL) //若没有则进入下一个套餐
                continue; 
            pay_package_update(&(para->lane[lane_id].pkg),package2);  
        }
    }
    if(cJsonData != NULL) cJSON_Delete(cJsonData);    //清除结构体

    vTaskDelay(500 / portTICK_PERIOD_MS);

    lyy_tjc_page_package(); //显示套餐

    cJSON* ack = NULL;
    ack = cJSON_CreateObject();
    if(ack != NULL)
    {
        cJSON_AddTrueToObject(ack,"result");
        thing_service_invoke_reply(ack, "success", id, "update_position_pay_package"); //应答
    }

    return 1;

}

/**
 * @description: 服务-更新货道信息
 * @param {*}
 * @return {*}
 */
int thing_service_lane_infor_set(cJSON* params, char *id)
{

    (void)id;
    //ndj_add
    uint8_t lane_id = 0;
    PLANE_INFOR_TABLE_DEF lane = ndj_net_lane_infor();
    uint8_t i,cnt;
    cJSON *temp = NULL;
    cJSON *data = NULL;

    if(params == NULL)
    {
        LYY_LOGD("lane infor error");
        return -1;
    }
    cJSON *postion = cJSON_GetObjectItem(params, "position_info");
    if(postion == NULL)
    {
        LYY_LOGD("no position");
        return -1;
    }
    cnt = cJSON_GetArraySize(postion);	//获取数组成员个数 
    cnt = (cnt >= lane->number ? lane->number : cnt);
    LYY_LOGD("lane_infor_set: %d",cnt);
    for(i=0;i<cnt;i++)
    {
        data = cJSON_GetArrayItem(postion, i);
        if(data == NULL || data->type != cJSON_String)
        {
            LYY_LOGD("lane data is error"); 
            continue;   
        }
        LYY_LOGD("ndj_lane_set: %s", data->valuestring);      
        cJSON *cJsonData = cJSON_Parse(data->valuestring);
        if(cJsonData == NULL)   
        {
            LYY_LOGD("cJSON_Parse fail");
            return -2;
        }
        temp = cJSON_GetObjectItem(cJsonData, "positionX");
        if(temp != NULL && temp->type == cJSON_Number && (temp->valueint >= 1))
        {
            lane_id = (temp->valueint -1); 
            if(lane_id >= NDJ_LANE_MAX) continue ; // 仓位ID过大
            lane->lane[lane_id].position_x = temp->valueint;
            LYY_LOGD("lane[%d].position_x = %d",lane_id,lane->lane[lane_id].position_x);
            LYY_LOGD("lane_id = %d; lane->number= %d",lane_id,lane->number);
            if(lane_id > lane->number)  continue ;
        }
        else  continue ;
        lane->flag[lane_id] = 0x01;  //更新 需要下发主板设置
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
            lane->lane[lane_id].enable = (temp->type == cJSON_False ? 0x00 : 0x01);
            LYY_LOGD("lane[%d].enable = %d",lane_id,lane->lane[lane_id].enable);
        } 
        temp = cJSON_GetObjectItem(cJsonData, "giftName");
        if(temp != NULL && temp->type == cJSON_String)
        {
            memset(lane->lane[lane_id].giftName,0,sizeof(lane->lane[lane_id].giftName));
            strcpy(lane->lane[lane_id].giftName,temp->valuestring);
            LYY_LOGD("lane[%d].giftName = %s",lane_id,lane->lane[lane_id].giftName);
        } 
    }

    vTaskDelay(500 / portTICK_PERIOD_MS);

    lyy_tjc_page_package(); //显示套餐

    return 1;
}
/***********************************ndj-end***************************/




/**
 * @brief 
 * 支付申请---按摩椅，娃娃机,脉冲
 * @param params 
 * @param id 
 * @return char 
 */
int thing_service_pay_apply_wwj(cJSON* params, char *id)
{


    cJSON *order = cJSON_GetObjectItem(params, "order");
    
    if((order == NULL) || (order->type != cJSON_String)) 
            return -1;

    return api_std_start_entryQue(START_TYPE_PAY_APPLY,0,order->valuestring,0,0,id); //支付申请入队

}

/**
 * @brief 
 * 支付申请应答---按摩椅，娃娃机,脉冲
 * @param id 
 * @param pay_id 
 * @param pay_status 
 * @param pack_id 
 */
void thing_service_reply_pay_apply(char *order,uint16_t result,char *pack_id)
{
    cJSON* data = NULL;
    data = cJSON_CreateObject();
    if(data != NULL)
    {
        cJSON_AddStringToObject(data, "order", order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        thing_service_invoke_reply(data, "success", pack_id, "pay_apply");
    }
}
/**
 * @brief 
 * 启动 --- 娃娃机，脉冲
 * @param params 
 * @param id 
 * @return char 
 */
int thing_service_pay_start_wwj(cJSON* params, char *id)
{

    cJSON *temp     = NULL;
    char *order     = NULL;
    uint32_t coins  = 0; 

    temp = cJSON_GetObjectItem(params, "number");
    if((temp != NULL) && (temp->type == cJSON_Number)) 
    {
        LYY_LOGD("number %ld", temp->valueint);
        coins = temp->valueint;
    }
    
    temp = cJSON_GetObjectItem(params, "order");
    if((temp == NULL) || (temp->type != cJSON_String)) 
        return -1;

    order = temp->valuestring;

    LYY_LOGD("order %s", temp->valuestring);

    return api_std_start_entryQue(START_TYPE_PAY_START,coins,order,0,0,id);

}

/**
 * @brief 
 * 启动应答 --wwj，脉冲
 * @param order 
 * @param result 
 * @param pack_id 
 */
void thing_service_reply_pay_wwj(char *order,uint16_t result,char *pack_id)
{
    cJSON* data = NULL;
    data = cJSON_CreateObject();
    if(data != NULL)
    {
        cJSON_AddStringToObject(data, "order", (char *)order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        thing_service_invoke_reply(data, "success", pack_id, "pay_start");
    }
}
/**
 * @brief 
 * 支付申请---兑币机
 * @param params 
 * @param id 
 * @return char 
 */
int thing_service_pay_apply_dbj(cJSON* params, char *id)
{

    cJSON *order = cJSON_GetObjectItem(params, "unique_code");
    if((order == NULL) || (order->type != cJSON_String)) return -1;

    return api_std_start_entryQue(START_TYPE_PAY_APPLY,0,order->valuestring,0,0,id); //支付申请入队


}


/**
 * @brief 
 * 支付申请应答---兑币机
 * @param id 
 * @param pay_id 
 * @param pay_status 
 * @param pack_id 
 */
void thing_service_reply_pay_apply_dbj(char *order,uint16_t result,char *pack_id)
{
    cJSON* data = NULL;
    data = cJSON_CreateObject();
    if(data != NULL)
    {
        cJSON_AddStringToObject(data, "unique_code", order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        thing_service_invoke_reply(data, "success", pack_id, "pay_apply");
    }
}

/**
 * @brief 
 * 启动 ---兑币机
 * @param params 
 * @param id 
 * @return char 
 */
int thing_service_pay_start_dbj(cJSON* params, char *id)
{

    int coins; 
    cJSON *temp     = NULL;
    char *order     = NULL;

    temp = cJSON_GetObjectItem(params, "pay_amount");

    if((temp != NULL) && (temp->type == cJSON_Number)) 
        LYY_LOGD("money %f", temp->valuedouble);
    
    temp = cJSON_GetObjectItem(params, "unique_code");
    if((temp == NULL) || (temp->type != cJSON_String)) 
        return -1;

    order = temp->valuestring;

    LYY_LOGD("order %s", temp->valuestring);

    temp = cJSON_GetObjectItem(params, "out_coins");
    if((temp == NULL) || (temp->type != cJSON_Number))  
        return -1;

    coins = temp->valueint;
    LYY_LOGD("out coins  %d", coins);

    return api_std_start_entryQue(START_TYPE_PAY_START, coins, order, 0, 0, id);

}

/**
 * @brief 
 * 启动应答 --兑币机
 * @param order 
 * @param result 
 * @param pack_id 
 */
void thing_service_reply_pay_start_dbj(char *order,uint16_t result,char *pack_id,uint16_t coins,double money)
{
    cJSON* data = NULL;
    data = cJSON_CreateObject();
    if(data != NULL)
    {
        cJSON_AddStringToObject(data, "unique_code", order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        cJSON_AddNumberToObject(data, "out_coins", coins);
        cJSON_AddNumberToObject(data, "pay_amount", money);
        thing_service_invoke_reply(data, "success", pack_id, "out_coins");
    }
}



/**
 * @brief 
 * 启动 -- 按摩椅
 * @param params 
 * @param id 
 * @return char 
 */
int thing_service_pay_start_amy(cJSON* params, char *id)
{

    cJSON *temp = NULL;
    int times; 
    char *order = NULL;

    temp = cJSON_GetObjectItem(params, "order");
    if((temp == NULL) || (temp->type != cJSON_String)) return -1;

    order = temp->valuestring;
    LYY_LOGD("order %s", temp->valuestring);

    temp = cJSON_GetObjectItem(params, "time");
    if((temp == NULL) || (temp->type != cJSON_Number))  return -1;
    times = temp->valueint;
    LYY_LOGD("times  %d", times);

    return api_std_start_entryQue(START_TYPE_PAY_START,times,order,0,0,id);

}
/**
 * @brief 
 * 启动应答 -- 按摩椅
 * @param order 
 * @param result 
 * @param pack_id 
 */
void thing_service_reply_pay_start_amy(char *order,uint16_t result,char *pack_id)
{
    cJSON* data = NULL;
    data = cJSON_CreateObject();
    if(data != NULL)
    {
        cJSON_AddStringToObject(data, "order", order);
        cJSON_AddStringToObject(data, "result", get_apply_start_result_type(result));
        thing_service_invoke_reply(data, "success", pack_id, "pay_start");
    }
}


/**
 * @brief 
 * 获取注册码
 * @param params 
 * @param id 
 * @return int 
 */
int thing_service_payment_code(cJSON* params, char *id)
{
    cJSON* url = NULL;

    url = cJSON_GetObjectItem(params, "url");
    if((url != NULL) && (url->type == cJSON_String))
    {
        LYY_LOGD("out coins unique_code %s", url->valuestring);

        // memset(pay_code, 0, sizeof(pay_code));
        // lyy_memcpy(pay_code, url->valuestring, strlen(url->valuestring));

    }

    //lyz test-测试直接回复成功
    thing_service_invoke_reply(NULL, "success", id, "pay_qr_code");
    return 0;

}

/**
 * @brief 
 * 下发动态二维码--通用
 * @param params 
 * @param id 
 * @return int 
 */
int thing_service_qrcode_dynamic_std(cJSON* params, char *id)
{


    cJSON* order = cJSON_GetObjectItem(params, "order");
    if(order != NULL&&order->type == cJSON_String) //启动订单号
    {
        LYY_LOGD("order: %s", order->valuestring);
    }
    else
    {
        LYY_LOGD("no order");
        return -1;
    }

    cJSON* url = cJSON_GetObjectItem(params, "url");
    if(url != NULL&&url->type == cJSON_String) //qrcode url
    {
        LYY_LOGD("url len=%d: %s", strlen(url->valuestring), url->valuestring);
    }
    else
    {
        LYY_LOGD("no url");
        return -1;
    }

    cJSON* money = cJSON_GetObjectItem(params, "money");
    if(money != NULL&&money->type == cJSON_String) //qrcode money
    {
        LYY_LOGD("money: %s", money->valuestring);
    }
    else
    {
        LYY_LOGD("no money");
        return -1;
    }

    cJSON *unique_code = cJSON_GetObjectItem(params, "unique_code");
    if(unique_code != NULL&&unique_code->type == cJSON_String) //指令唯一码
    {
        LYY_LOGD("unique_code: %s", unique_code->valuestring);

        //屏幕上做显示处理
        if(cmp_pay_url_uinque_code(0,unique_code->valuestring)==0x01) return -1; 

    }
    else
    {
        LYY_LOGD("no unique_code");//没有支付唯一码 做显示

        return -2;
    }

    //PAY_PARAM_DEF *param = get_pay_param();
    //lyy_tjc_pay_page_set(pay_url_wallet_number_get(),param->chnl_sum,url->valuestring,money->valuestring);//显示支付二维码

    lyy_tjc_pay_url_show(url->valuestring,money->valuestring);


    thing_service_invoke_reply(NULL, "success", id, "qrcode_post"); //应答

    return 1;
}

/**
 * @brief 
 * 扭蛋机
 * @param params 
 * @param id 
 * @return char 
 */
char thing_service_qrcode_dynamic_ndj(cJSON* params, char *id)
{
    //ndj_add
    uint8_t lane_id;
    cJSON* order = cJSON_GetObjectItem(params, "order");
    if(order != NULL&&order->type == cJSON_String) //启动订单号
    {
        LYY_LOGD("order: %s", order->valuestring);
    }
    else
    {
        LYY_LOGD("no order");
        return 0;
    }

    cJSON* url = cJSON_GetObjectItem(params, "url");
    if(url != NULL&&url->type == cJSON_String) //qrcode url
    {
        LYY_LOGD("url len=%d: %s", strlen(url->valuestring), url->valuestring);
    }
    else
    {
        LYY_LOGD("no url");
        return 0;
    }

    cJSON* money = cJSON_GetObjectItem(params, "money");
    if(money != NULL&&money->type == cJSON_String) //qrcode money
    {
        LYY_LOGD("money: %s", money->valuestring);
    }
    else
    {
        LYY_LOGD("no money");
        return 0;
    }

    cJSON* position_x = cJSON_GetObjectItem(params, "position_x");
    if(position_x != NULL&&position_x->type == cJSON_Number) //qrcode money
    {
        LYY_LOGD("position_x: %d", position_x->valueint);
        lane_id = position_x->valueint;
    }
    else
    {
        LYY_LOGD("no position_x");
        return -1;
    }

    cJSON *unique_code = cJSON_GetObjectItem(params, "unique_code");
    if(unique_code != NULL&&unique_code->type == cJSON_String) //指令唯一码
    {
        LYY_LOGD("unique_code: %s", unique_code->valuestring);
        //屏幕上做显示处理
        if(cmp_pay_url_uinque_code(lane_id,unique_code->valuestring)==0x01) return -1; 
    }
    else
    {
        LYY_LOGD("no unique_code");
    }

    // PAY_PARAM_DEF *param =get_pay_param();

    // lyy_tjc_pay_page_set(pay_url_wallet_number_get(),param->chnl_sum,url->valuestring,money->valuestring);
    lyy_tjc_pay_url_show(url->valuestring,money->valuestring);

    vTaskDelay(5 / portTICK_PERIOD_MS);

    if(ndj_infor_t.dzy_flag == 0x01)
    { 
        ndj_dzy_get_qrcode_ack(lane_id,url->valuestring);
    }  

    thing_service_invoke_reply(NULL, "success", id, "qrcode_post"); //应答

    return 1;
}
/**
 * @brief 
 * 获取动态二维码
 * @param params 
 * @param id 
 * @return int 
 */
int thing_service_qrcode_dynamic(cJSON* params, char *id)
{
    if(get_dev_type_code()==DEVICE_TYPE_UART_NDJ) //扭蛋机
    {
        thing_service_qrcode_dynamic_ndj(params,id);
    }
    else
    {
        thing_service_qrcode_dynamic_std(params,id);    
    }
    return 1;
}

/**
 * @brief 
 * 支付结果下发
 * @param params 
 * @param id 
 * @return int 
 */

int thing_service_pay_result(cJSON* params, char *id)
{
    cJSON* result = cJSON_GetObjectItem(params, "status");
    if(result != NULL&&result->type == cJSON_String) //启动订单号
    {
        LYY_LOGD("status: %s", result->valuestring);
    }
    else
    {
        LYY_LOGD("no order");
        return -1;
    }
    

    lyy_tjc_page_pay_reuslt(); //屏幕上做显示处理

    cJSON* data = NULL;
    data = cJSON_CreateObject();
    if(data != NULL)
    {
        cJSON_AddStringToObject(data, "result", "success");
        thing_service_invoke_reply(data, "success", id, "pay_notice"); //应答
    }
    
    return 1;
}


/**
 * @description: 服务-重启
 * @param {*}
 * @return {*}
 */
int thing_service_restart(cJSON* params, char *id)
{
    if(params == NULL)
    {
        LYY_LOGD("para error");
        return -1;
    }
    cJSON *obj = cJSON_GetObjectItem(params, "obj");
    if(obj != NULL && obj->type == cJSON_String)
    {
        cJSON* data = NULL;
        data = cJSON_CreateObject();
        if(data != NULL)
        {
            cJSON_AddStringToObject(data, "result", "success");
            thing_service_invoke_reply(data, "success", id, "reboot"); //应答
        }
        LYY_LOGD("obj->string = %s",obj->valuestring);
        if(strstr(obj->valuestring,"box")) 
        {
            HAL_vReset("system restart", 1000);
        }
    } 
    return 1;
}

/**
 * @description: 绑定通知
 * @param {*}
 * @return {*}
 */
int thing_service_equipment_binding(cJSON *params, char *id)
{
    (void)params;
    (void)id;
    if (get_dev_type_code() == DEVICE_TYPE_UART_DFSSJ)
    {
        device_dfssj_bind_state_info(1); // 扫码+刷卡
    }
    return 1;
}

/**
 * @description: 解绑通知
 * @param {*}
 * @return {*}
 */
int thing_service_equipment_unbinding(cJSON *params, char *id)
{
    (void)params;
    (void)id;
    if (get_dev_type_code() == DEVICE_TYPE_UART_DFSSJ)
    {
        device_dfssj_bind_state_info(0); // 解绑
    }
    return 1;
}

