/*
 * @author: e.he
 * @Date: 2024-06-14 10:53:53
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-09 14:46:01
 * @Description: 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>



#include "HAL_Log.h"
#include "HAL_Http.h"
#include "HAL_Uart.h"
#include "HAL_Flash.h"
#include "HAL_System.h"

#include "APP_Param.h"
#include "APP_Upgrade.h"


#include "lyy_pic.h"
#include "lyy_para.h"
#include "lyy_device.h"

#include "thing_iot.h"
#include "thing_mqtt.h"
#include "thing_init.h"
#include "thing_event.h"
#include "thing_service.h"
#include "thing_property.h"

#include "utils.h"




int lyy_thing_down_property(char *payload, uint32_t payload_len);
int lyy_thing_event_reply(char *payload, uint32_t payload_len);
int lyy_thing_down_service(char *payload, uint32_t payload_len);
int lyy_thing_down_ota(char *payload, uint32_t payload_len);
int lyy_thing_down_ctlb(char *payload, uint32_t payload_len);
int lyy_thing_down_devInfor_reply(char *payload, uint32_t payload_len);
int lyy_thing_raw_down(char *payload, uint32_t payload_len);

void thing_build_topic_name(char *topic,char *name);



/**
 * @brief Get the random string object
 * 获取随机字符串
 * @return char* 
 */
char *get_random_string(void)
{
    char        id[6]       = {0x00,};
    static char id_str[15]  = {0x00,};

    vRandomGet(id, sizeof(id));
    utils_lhexToString(id_str, (const unsigned char *)id, sizeof(id));

    return id_str;

}


/**
 * @brief 
 * 下行数据处理
 * @param payload 
 * @param payload_len 
 * @param topic 
 * @return lyy_int32_t 
 */
void lyy_thing_down_parse(char *payload, uint32_t payload_len, char *topic)
{

    if(payload == NULL)     
        return ;
    if(payload_len == 0)    
        return ;
    if(topic == NULL)       
        return ;
        
    LYY_LOGD("lyy_thing_down_parse topic %s", topic);
    if(strstr(topic, "thing/property/") != NULL)//属性
    {
        lyy_thing_down_property(payload, payload_len);
    }
    else if(strstr(topic, "thing/event/") != NULL)//事件
    {
        lyy_thing_event_reply(payload, payload_len);
    }
    else if(strstr(topic, "thing/service/") != NULL)//服务
    {
        lyy_thing_down_service(payload, payload_len);
    }
    else if(strstr(topic, "ota/") != NULL)//升级
    {
        lyy_thing_down_ota(payload, payload_len);
    }
    else if(strstr(topic, "ctlb/model/down_raw") != NULL)//参数下行
    {
        lyy_thing_down_ctlb(payload, payload_len);
    }
    else if(strstr(topic, "ctlb/model/up_raw_reply") != NULL)//参数上行
    {
        LYY_LOGD("get ### up_raw_reply ###");
    }
    else if(strstr(topic, "thing/deviceinfo_reply") != NULL)//设备标签应答
    {
        lyy_thing_down_devInfor_reply(payload,payload_len);
    }   else if(strstr(topic, "raw/down") != NULL)//反向下行
    {
        lyy_thing_raw_down(payload, payload_len);
    }
    else if(strstr(topic, "raw/up") != NULL)//反向上行
    {
        LYY_LOGD("get ### up_raw_reply ###");
    }

}


/**
 * @brief 
 * 
 * @param member 
 */
void dev_type_update(cJSON *member)
{

    if(member == NULL) 
        return ; //成员为空   

    cJSON *temp = cJSON_GetObjectItem(member, "dev_type");

    if(temp != NULL && temp->type == cJSON_String) 
    {
        LYY_LOGD("dev_type1: %s ", temp->valuestring);
        LYY_LOGD("dev_type2: %s ", APP_pcGetDevType());

        if(memcmp(temp->valuestring, APP_pcGetDevType(), strlen(temp->valuestring)) != 0) //不一样
        {
            APP_lserSetDevTypeInfo(temp->valuestring);
            HAL_vReset("update dev_type to reset!!", 1000);
        }

    }

}

/**
 * @brief 
 * 设备标签应答
 * @param payload 
 * @param payload_len 
 * @return int 
 */
int lyy_thing_down_devInfor_reply(char *payload, uint32_t payload_len)
{

    (void)payload_len;

    cJSON *member       = NULL;
    cJSON *cJsonData    = cJSON_Parse(payload);

    if(cJsonData == NULL)   
        return -2;

    cJSON* id = cJSON_GetObjectItem(cJsonData, "id");
    if((id != NULL) && (id->type == cJSON_String))
    {
        LYY_LOGD("id: %s", id->valuestring);
    }

    cJSON* code = cJSON_GetObjectItem(cJsonData, "code");
    if((code != NULL) && (code->type == cJSON_String))
    {
        LYY_LOGD("code: %s", code->valuestring);
    }

    cJSON* msg = cJSON_GetObjectItem(cJsonData, "message");
    if((msg != NULL) && (msg->type == cJSON_String))
    {
        LYY_LOGD("message: %s", msg->valuestring);
        if(strstr(msg->valuestring,"success")==NULL) //成功则更新
        {
            cJSON_Delete(cJsonData);
            return -1;
        }
            
    }

    cJSON* data = cJSON_GetObjectItem(cJsonData, "data");
    if((data != NULL) && (data->type == cJSON_Array))
    {
        int sub_size = cJSON_GetArraySize(data);	//获取数组成员个数 
        LYY_LOGD("sub_size :%d",sub_size);
        for(int i=0;i<sub_size;i++)
        {
            member = cJSON_GetArrayItem(data, i);
            dev_type_update(member); 
        }
    }

    cJSON_Delete(cJsonData);

    return 1;

}


/**
 * @brief 
 * 设备属性获取
 * @param params 
 */
void thing_devInfor_query(cJSON* params)
{

    char topic[128]     = {0x00,};
    cJSON* cjsonData    = NULL;
    char *pcData        = NULL;
    char id[6]          = {0x00,};
    char id_str[15]     = {0x00,};

    memset(topic, 0, sizeof(topic));
    cjsonData = cJSON_CreateObject();

    if(cjsonData != NULL && params != NULL)
    {
        //id-相当于指令唯一码
        vRandomGet(id, sizeof(id));
        utils_lhexToString(id_str, (const unsigned char*)id, sizeof(id));
        cJSON_AddStringToObject(cjsonData, "id", id_str);
        //params
        cJSON_AddItemToObject(cjsonData, "params", params);
        //version-协议版本
        cJSON_AddStringToObject(cjsonData, "version", "1.0");
        cJSON_AddStringToObject(cjsonData, "method", "thing.deviceinfo.get");

        pcData = cJSON_PrintUnformatted(cjsonData);
        if(pcData != NULL)
        {
            memset(topic,0,sizeof(topic));
            thing_build_topic_name(topic,"thing/deviceinfo");
            lyy_mqtt_model_publish(topic, (unsigned char *)pcData, strlen(pcData));
            free(pcData);
        }
    }else{

        if(params != NULL) {
            cJSON_Delete(params);
        }
    }

    if(cjsonData != NULL)
    {
        cJSON_Delete(cjsonData);
    }

}

/**
 * @brief 
 * 获取设备属性
 */
void thing_device_infor_query(void)
{

    cJSON* params = cJSON_CreateArray();
    LYY_LOGD("thing_device_infor_query");

    if(params == NULL) 
        return ;

    cJSON* member = cJSON_CreateObject();

    if(member == NULL)
    {
        cJSON_Delete(params);
        return ;
    }
        
    cJSON_AddStringToObject(member, "dev_type", "1");
    cJSON_AddItemToArray(params, member);
    thing_devInfor_query(params);    

}

/**
 * @brief 
 * 
 * @param topic 
 * @param name 
 */
void thing_build_topic_name(char *topic,char *name)
{

    sprintf(topic ,"sys/%s/%s/%s", APP_pcGetProductKey(),
                                   APP_pcGetParamDevId(),
                                   name);
}

/**
 * @description: 设备正向透传数据下行应答，payload无内容 sys/${productKey}/${deviceId}/ctlb/model/down_raw_reply
 * @param {*}
 * @return {*}
 */
void thing_ctlb_down_raw_reply(void)
{

    char topic[100] = {0x00, };
    unsigned char data = 0x01;//0x01-成功 0x00-失败      
    memset(topic, 0, sizeof(topic));

    thing_build_topic_name(topic,"ctlb/model/down_raw_reply");
    lyy_mqtt_model_publish(topic, &data, sizeof(data));

}

/**
 * @description: 物模型ctlb处理-下行
 * @param {*}
 * @return {*}
 */
int lyy_thing_down_ctlb(char *payload, uint32_t payload_len)
{

    uint8_t cmd     = *payload;
    uint8_t *data   = NULL;

    if(payload_len > 1) 
    {
        data = (uint8_t *)payload + 1;
    }


    //发送到串口
    device_packet_data_length(cmd, data, payload_len-1);


    //应答-down_raw_reply
    thing_ctlb_down_raw_reply();

    return 0;

}


/**
 * @description: 设备反向透传数据下行应答，payload无内容 sys/${productKey}/${deviceId}/raw/down
 * @param {*}
 * @return {*}
 */
void thing_raw_down_reply(void)//
{

    char topic[100] = {0x00,};
    unsigned char data = 0x01;//0x01-成功 0x00-失败      
    memset(topic,0,sizeof(topic));
    thing_build_topic_name(topic,"raw/down");
    lyy_mqtt_model_publish(topic, &data, sizeof(data));

}


/**
 * @description: 物模型ctlb处理-下行
 * @param {*}
 * @return {*}
 */
int lyy_thing_raw_down(char *payload, uint32_t payload_len)
{
    uint8_t *data = NULL;
    if(payload_len > 6) //有数据区
        data = (uint8_t *)payload + 6;

    // 串口发送
    HAL_vUartSend(HAL_UART_MAIN_BOARD, data,payload_len-6, 800);

    //应答-down_raw_reply
    thing_raw_down_reply();
    
    return 0;
}

/**
 * @description: 设备反向透传数据上行应答，payload无内容 sys/${productKey}/${deviceId}/raw/up
 * @param {*}
 * @return {*}
 */
void lyy_thing_raw_up(uint8_t *data, uint16_t len)
{
    if(data == NULL)    return;
    if(len == 0)        return;

    char topic[100] = {0x00,};    

    memset(topic, 0, sizeof(topic));
    thing_build_topic_name(topic,"raw/up");

    lyy_mqtt_model_publish(topic, (unsigned char *)data, len);
}


/**
 * @description: BC指令
 * @param {*}
 * @return {*}
 */
void lyy_thing_ctlb_up_raw(uint8_t *data, uint16_t len)
{
    if(data == NULL)    return;
    if(len == 0)        return;

    char topic[100] = {0x00,};    

    memset(topic, 0, sizeof(topic));
    thing_build_topic_name(topic,"ctlb/model/up_raw");

    lyy_mqtt_model_publish(topic, (unsigned char *)data, len);

}

/**
 * @brief 
 * 主板故障
 * @param err 
 */
void ctlb_up_raw_board_error(uint8_t err)
{
    uint8_t data[2];

    data[0] = 0xFF; //0xff代表主板错误码
    data[1] = err;

    lyy_thing_ctlb_up_raw(data, sizeof(data));
}


static uint32_t g_ulWriteOffset;

static void APP_prvBoardUpdataDataRvCd(char *pcData, uint32_t ulLen)
{

    int lRet = HAL_vFileWriteOffset(THING_IOT_BOARD_PATH, 
                                    (uint8_t *)pcData, 
                                    ulLen,
                                    g_ulWriteOffset);
    
    if(lRet < 0 || lRet != (int)ulLen) {

        LYY_LOGE("###  write fail");
        return ;
    } 

    LYY_LOGD(">>> board ota write datalen : [%d]   offset :[%d]", ulLen, g_ulWriteOffset);

    g_ulWriteOffset += ulLen;

}



static void APP_prvBoardUpdataRes(uint8_t ucRes, uint32_t ulLen)
{
    (void)ucRes;

    LYY_LOGD("HTTP RECV DATA len: [%d]", ulLen);

    if(HTTP_GET_SUCCESS == ucRes) {

        if(g_ulWriteOffset == ulLen) {

            LYY_LOGD(" board write succes :[%s]", THING_IOT_BOARD_PATH);
            board_ota_status_send();

        }else{

            HAL_vFileClear(THING_IOT_BOARD_PATH);
            g_ulWriteOffset = 0x00;
        }

    }else{

        HAL_vFileClear(THING_IOT_BOARD_PATH);
        g_ulWriteOffset = 0x00;
    }

}


/**
 * @description: 物模型ota处理
 * @param {*}
 * @return {*}
 */
int lyy_thing_down_ota(char *payload, uint32_t payload_len)
{
    if((payload == NULL) || (payload_len == 0)) return -1;

    cJSON* cJsonData = NULL;
    cJSON* code     = NULL;
    cJSON* id       = NULL;
    cJSON* data     = NULL;

    //ota数据内容
    cJSON* size     = NULL;
    cJSON* module   = NULL;
    cJSON* url      = NULL;
    cJSON* version  = NULL;


    cJsonData = cJSON_Parse(payload);
    if(cJsonData == NULL)   return -2;

    code = cJSON_GetObjectItem(cJsonData, "code");
    if((code != NULL) && (code->type == cJSON_Number))
    {
        LYY_LOGD("get ota url code %d", code->valueint);
    }

    id = cJSON_GetObjectItem(cJsonData, "id");
    if((id != NULL) && (id->type == cJSON_String))
    {
        LYY_LOGD("get ota url id %s", id->string);
    }

    data = cJSON_GetObjectItem(cJsonData, "data");
    if(data == NULL)
    {
        cJSON_Delete(cJsonData);//清除结构体
        LYY_LOGD("get ota data error");
        return -3;
    }

    //ota url下发内容解析
    size = cJSON_GetObjectItem(data, "size");
    if((size != NULL) && (size->type == cJSON_Number))
    {
        LYY_LOGD("get ota file size %d", size->valueint);
    }    


    url = cJSON_GetObjectItem(data, "url");
    if((url == NULL) || (url->type != cJSON_String))
    {
        cJSON_Delete(cJsonData);//清除结构体
        LYY_LOGE("get ota URL!!!");
        return -4;
    }
    LYY_LOGD("get ota url  %s", url->valuestring);

    module = cJSON_GetObjectItem(data, "module");
    if((module == NULL) || (module->type != cJSON_String))
    {
        cJSON_Delete(cJsonData);//清除结构体
        LYY_LOGE("get ota module error!!!");
        return -5;
    }

    LYY_LOGD("get ota module  %s", module->valuestring);

    version = cJSON_GetObjectItem(data, "version");
    if((version == NULL) || (version->type != cJSON_String))
    {
        cJSON_Delete(cJsonData);//清除结构体
        LYY_LOGE("get ota version error!!!");
        return -6;
    }

    LYY_LOGD("get ota version  %s", version->valuestring);  


    if(strstr("board", module->valuestring) != NULL) {

        board_ota_param_init(version->valuestring,NULL,size->valueint);

        g_ulWriteOffset = 0x00;
        HAL_vFileClear(THING_IOT_BOARD_PATH);

        //异步不了， 资源不够
        APP_UpgradeDelNetWorkDev(   APP_UPGRADE_HTTP, 
                                    APP_UPGRADE_DEVICE,  
                                    (uint8_t *)url->valuestring, 
                                    strlen(url->valuestring),
                                    APP_prvBoardUpdataDataRvCd,
                                    APP_prvBoardUpdataRes);
    }
    else {

        //异步不了， 资源不够
        APP_UpgradeDelNetWorkDev(   APP_UPGRADE_HTTP, 
                                    APP_UPGRADE_APP,  
                                    (uint8_t *)url->valuestring, 
                                    strlen(url->valuestring),
                                    NULL,
                                    NULL);
    }

    cJSON_Delete(cJsonData);//清除结构体
    
    return 0;
}

/**
 * @description: ota-上报设备版本
 * @param {*}module-default:盒子 board:主板
 * @return {*}
 */
void thing_ota_inform_version(char *version, char* module)
{

    cJSON* cjsonData    = NULL;
    cJSON* params       = NULL;
    cJSON* params_para  = NULL;
    char *pcData        = NULL;

    char id[6]          = {0x00,};
    char id_str[20]     = {0x00,};
    char topic[128]     = {0x00,};

    cjsonData   = cJSON_CreateObject();
    if(!cjsonData) 
    {
        LYY_LOGE("cjsonData create fail");
        return ;
    }

    params_para = cJSON_CreateObject();
    if(!params_para) 
    {
        cJSON_Delete(cjsonData);
        LYY_LOGE("params_para create fail");
        return ;
    }

    params = cJSON_CreateArray();
    if(!params)
    {
        cJSON_Delete(params_para);
        cJSON_Delete(cjsonData);
        LYY_LOGE("params create fail");
        return ;
    }

    //id-相当于指令唯一码
    vRandomGet(id, sizeof(id));
    utils_lhexToString(id_str, (const unsigned char*)id, sizeof(id));
    cJSON_AddStringToObject(cjsonData, "id", id_str);

    //软件版本
    cJSON_AddStringToObject(params_para, "version", version);
    cJSON_AddStringToObject(params_para, "module", module);
    cJSON_AddItemToArray(params, params_para);

    cJSON_AddItemToObject(cjsonData, "params", params);

    pcData = cJSON_PrintUnformatted(cjsonData);

    if(pcData != NULL)
    {
        memset(topic, 0, sizeof(topic));
        thing_build_topic_name(topic,"ota/device/inform");

        lyy_mqtt_model_publish(topic, (unsigned char *)pcData, strlen(pcData));
        free(pcData);

    }
        
    cJSON_Delete(cjsonData);

}



/**
 * @description: 物模型事件处理
 * @param {*}
 * @return {*}
 */
int lyy_thing_event_reply(char *payload, uint32_t payload_len)
{

    if((payload == NULL) || (payload_len == 0)) return -1;

    cJSON* cJsonData = NULL;
    cJSON* method = NULL;
    cJSON* message = NULL;

    cJsonData = cJSON_Parse(payload);
    if(cJsonData == NULL)   
        return -2;

    method = cJSON_GetObjectItem(cJsonData, "method");
    if((method == NULL) || (method->type != cJSON_String))
    {
        cJSON_Delete(cJsonData);
        LYY_LOGD("get event method error");
        return -3;
    }
    LYY_LOGD("get event method:%s", method->valuestring);

    message = cJSON_GetObjectItem(cJsonData, "message");
    if((message == NULL) || (message->type != cJSON_String))
    {
        cJSON_Delete(cJsonData);
        LYY_LOGD("get event message error");
        return -4;
    }

    LYY_LOGD("get para message:%s", message->valuestring);

    cJSON_Delete(cJsonData);//清除结构体

    return 0;
}

/**
 * @description: 物模型服务处理
 * @param {*}
 * @return {*}
 */
int lyy_thing_down_service(char *payload, uint32_t payload_len)
{
    if((payload == NULL) || (payload_len == 0)) return -1;

    cJSON* cJsonData = NULL;
    cJSON* method = NULL;
    cJSON* id   = NULL;
    cJSON* params = NULL;

    cJsonData = cJSON_Parse(payload);
    if(cJsonData == NULL)   
        return -2;

    method = cJSON_GetObjectItem(cJsonData, "method");
    if((method == NULL) || (method->type != cJSON_String))
    {
        cJSON_Delete(cJsonData);//清除结构体
        LYY_LOGD("get service method error");
        return -3;
    }
    LYY_LOGD("service method:%s", method->valuestring);

    params = cJSON_GetObjectItem(cJsonData, "params");
    if(params == NULL)
    {
        cJSON_Delete(cJsonData);//清除结构体
        LYY_LOGD("get sevvice params error");
        return -3;
    }

    id = cJSON_GetObjectItem(cJsonData, "id");
    if(id == NULL)
    {
        cJSON_Delete(cJsonData);//清除结构体
        LYY_LOGD("get sevvice id error");
        return -3;
    }

    LYY_LOGD("service parse");
    thing_service_parse(params,method->valuestring,id->valuestring);

    if(cJsonData != NULL)
    {
        cJSON_Delete(cJsonData);//清除结构体
    }

    return 0;
}

/**
 * @brief 
 * 服务应答
 * @param data 
 * @param message 
 * @param id 
 * @param identifier 
 */
void thing_service_invoke_reply(cJSON* data, char *message, char *id, char *identifier)
{

    char topic[100] = {0x00,};
    char method[40] = {0x00,};
    char temp[100]  = {0x00,};

    cJSON* cjsonData    = NULL;
    char *pcData        = NULL;
    cjsonData = cJSON_CreateObject();

    if(cjsonData != NULL)
    {
        //code-200
        cJSON_AddNumberToObject(cjsonData, "code", 200);
        //id-相当于指令唯一码
        cJSON_AddStringToObject(cjsonData, "id", id);
        //params
        if(data != NULL)
        {
            cJSON_AddItemToObject(cjsonData, "data", data);
        }  

        //version-协议版本
        cJSON_AddStringToObject(cjsonData, "version", "1.0");
        //message-消息
        cJSON_AddStringToObject(cjsonData, "message", message);
        //message-消息
        sprintf(method ,"thing.service.%s", identifier);
        cJSON_AddStringToObject(cjsonData, "method", method);

        pcData = cJSON_PrintUnformatted(cjsonData);
        if(pcData != NULL)
        {
            memset(temp, 0, sizeof(temp));
            sprintf(temp ,"thing/service/%s/invoke_reply",identifier);
            memset(topic,0,sizeof(topic));
            thing_build_topic_name(topic,temp);
            lyy_mqtt_model_publish(topic, (unsigned char *)pcData, strlen(pcData));
            free(pcData);
        }

        cJSON_Delete(cjsonData);

    }else{

        cJSON_Delete(data);
    }
}


/*********                  thing 属性                   *************/
/**
 * @description: 设备属性上报
 * （sys/${productKey}/{deviceId}/thing/property/post）
 * @param {*}
 * @return {*}
 */
void thing_property_post(cJSON* params)
{
    char topic[100]     = {0x00,};
    cJSON* cjsonData    = NULL;
    char *pcData        = NULL;
    char id[6]          = {0x00,};
    char id_str[15]     = {0x00,};
    
    cjsonData = cJSON_CreateObject();

    if(cjsonData != NULL)
    {
        // LYY_LOGD("pro post cJSON_CreateObject OK!!!!");
        //id-相当于指令唯一码
        vRandomGet(id, sizeof(id));
        utils_lhexToString(id_str, (const unsigned char*)id, sizeof(id));
        
        cJSON_AddStringToObject(cjsonData, "id", id_str);
        //params
        cJSON_AddItemToObject(cjsonData, "params", params);
        //version-协议版本
        cJSON_AddStringToObject(cjsonData, "version", "1.0");
        //method
        cJSON_AddStringToObject(cjsonData, "method", "thing.event.property.post");

        pcData = cJSON_PrintUnformatted(cjsonData);
        if(pcData != NULL)
        {
            memset(topic, 0, sizeof(topic));
            thing_build_topic_name(topic,"thing/property/post");
            lyy_mqtt_model_publish(topic, (unsigned char *)pcData, strlen(pcData));
            free(pcData);
        }

        cJSON_Delete(cjsonData);    
    }else{

        cJSON_Delete(params);
    }


}


/**
 * @description: 设备属性读取应答
 * @param {*}
 * @return {*}
 */
void property_read_reply(cJSON* params, char *message)//
{
    char topic[100]     = {0x00,};
    cJSON* cjsonData    = NULL;
    char *pcData        = NULL;
    char id[6]          = {0x00,};
    char id_str[15]     = {0x00,};

    cjsonData = cJSON_CreateObject();
    if(cjsonData != NULL)
    {

        //code-200
        cJSON_AddNumberToObject(cjsonData, "code", 200);

        //id-相当于指令唯一码
        vRandomGet(id, sizeof(id));
        utils_lhexToString(id_str, (const unsigned char*)id, sizeof(id));

        cJSON_AddStringToObject(cjsonData, "id", id_str);
        //params
        cJSON_AddItemToObject(cjsonData, "data", params);
        //version-协议版本
        cJSON_AddStringToObject(cjsonData, "version", "1.0");
        //message-消息
        cJSON_AddStringToObject(cjsonData, "message", message);
        //method
        cJSON_AddStringToObject(cjsonData, "method", "thing.service.property.read");

        pcData = cJSON_PrintUnformatted(cjsonData);
        if(pcData != NULL)
        {
            thing_build_topic_name(topic,"thing/property/read_reply");
            lyy_mqtt_model_publish(topic, (unsigned char *)pcData, strlen(pcData));
            free(pcData);
        }

        cJSON_Delete(cjsonData);
    }else{

        cJSON_Delete(params);
    }


}

/**
 * @brief 
 * 属性设置应答
 * @param msg 
 * @param id 
 */
void thing_property_set_reply(char *msg,char *id)
{
    
    char topic[100]     = {0x00,};
    cJSON* cjsonData    = NULL;
    char *pcData        = NULL;

    cjsonData = cJSON_CreateObject();
    if(cjsonData != NULL)
    {

        //code-200
        cJSON_AddNumberToObject(cjsonData, "code", 200);
        // //id-相当于指令唯一码

        cJSON_AddStringToObject(cjsonData, "id", id);
        cJSON *reply = cJSON_CreateObject();
        if(reply != NULL)
        {
            cJSON_AddStringToObject(reply, "status", msg);
        }      
        //params
        cJSON_AddItemToObject(cjsonData, "data", reply);
        //version-协议版本
        cJSON_AddStringToObject(cjsonData, "version", "1.0");
        //message-消息
        cJSON_AddStringToObject(cjsonData, "message", msg);

        pcData = cJSON_PrintUnformatted(cjsonData);
        if(pcData != NULL)
        {
            memset(topic, 0, sizeof(topic));
            thing_build_topic_name(topic,"thing/property/set_reply");
            lyy_mqtt_model_publish(topic, (unsigned char *)pcData, strlen(pcData));
            free(pcData);
        }

        cJSON_Delete(cjsonData);

    }

}

/**
 * @brief 
 *  属性
 * @param payload 
 * @param payload_len 
 * @return int 
 */
int lyy_thing_down_property(char *payload, uint32_t payload_len)
{

    #define METHOD_PRO_REPLY    "thing.event.property.post"
    #define METHOD_PRO_SET      "thing.service.property.set"
    #define METHOD_PRO_READ     "thing.service.property.read"

    if((payload == NULL) || (payload_len == 0)) return -1;

    cJSON* cJsonData    = NULL;
    cJSON* method       = NULL;
    cJSON* message      = NULL;
    cJSON* params       = NULL;
    cJSON* code         = NULL;

    cJsonData = cJSON_Parse(payload);
    if(cJsonData == NULL)   
        return -2;

    method = cJSON_GetObjectItem(cJsonData, "method");
    if((method == NULL) || (method->type != cJSON_String))
    {
        LYY_LOGD("property method error");
        cJSON_Delete(cJsonData);//清除结构体
        return -3;
    }

    LYY_LOGD("property method:%s", method->valuestring);

    if(strstr(method->valuestring, METHOD_PRO_REPLY) != NULL)//属性上报应答
    {
        message = cJSON_GetObjectItem(cJsonData, "message");
        if((message != NULL) && (message->type == cJSON_String))
        {
            LYY_LOGD("property reply message:%s", message->valuestring);
        }

        code = cJSON_GetObjectItem(cJsonData, "code");
        if((code != NULL) && (code->type == cJSON_String))
        {
            LYY_LOGD("property reply code:%s", code->valuestring);
        } 
    }
    else if(strstr(method->valuestring, METHOD_PRO_SET) != NULL)//属性设置下发
    {
        params = cJSON_GetObjectItem(cJsonData, "params");
        if(params != NULL)
        {
            thing_property_set(params);
        }
    }
    else if(strstr(method->valuestring, METHOD_PRO_READ) != NULL)//属性读取
    {
        params = cJSON_GetObjectItem(cJsonData, "params");
        if(params != NULL)
        {
            thing_property_read(params);
        }
    }

    cJSON_Delete(cJsonData);//清除结构体

    return 1;

}

/****************************** 属性 end ***************************************/

/**
 * @description: 设备事件上报（sys/${productKey}/{deviceId}/thing/event/${event.identifier}/post）
 * @param {*}
 * @return {*}
 */
void thing_event_post(cJSON* value, char *event)
{

    char topic[256]     = {0x00, };
    char method[50]     = {0x00, };
    cJSON* cjsonData    = NULL;
    cJSON* params       = NULL;
    char *pcData        = NULL;
    char id[6]          = {0x00,};
    char id_str[15]     = {0x00,};
    char temp[128]      = {0};
   
    
    cjsonData   = cJSON_CreateObject();
    if(!cjsonData)
    {
        cJSON_Delete(value);
        return ;
    }

    params      = cJSON_CreateObject();
    if(!params)
    {
        cJSON_Delete(value);
        cJSON_Delete(cjsonData);
        return ;
    }


    //id-相当于指令唯一码
    vRandomGet(id, sizeof(id));
    utils_lhexToString(id_str, (const unsigned char*)id, sizeof(id));
    cJSON_AddStringToObject(cjsonData, "id", id_str);

    //params
    cJSON_AddItemToObject(params,   "value",  value);
    cJSON_AddNumberToObject(params, "time",   HAL_ulGetTimeMs()/1000);//lyz test需要更换成可变的
    cJSON_AddItemToObject(cjsonData,"params", params);

    //version-协议版本
    cJSON_AddStringToObject(cjsonData, "version", "1.0");

    //method
    sprintf(method, "thing.event.%s.post", event);
    cJSON_AddStringToObject(cjsonData, "method", method);

    pcData = cJSON_PrintUnformatted(cjsonData);
    if(pcData != NULL)
    {
        sprintf(temp ,"thing/event/%s/post",event);
        thing_build_topic_name(topic,temp);
        lyy_mqtt_model_publish(topic, (unsigned char *)pcData, strlen(pcData));
        free(pcData);
    }

    cJSON_Delete(cjsonData);
  
}






