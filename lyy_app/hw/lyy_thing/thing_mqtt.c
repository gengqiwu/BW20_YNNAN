/**
 * @file thing_mqtt.c
 * @brief MQTT 连接、订阅、心跳及客户端阶段状态管理。
 */
/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2022-05-27 17:33:56
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 21:03:43
 * @FilePath: \lyy_app_wwj\lyy_app\user\lyy_v1\lmq\src\lyy_lmq_client.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "HAL_Log.h"
#include "HAL_System.h"

#include "mqtt_api.h"
#include "thing_mqtt.h"

#include "thing_iot.h"
#include "thing_init.h"

#include "APP_Param.h"
#include "APP_NetWork.h"


#define LYY_LMQ_QOS                (0)
#define TOPIC_END_NUM              (32U)
#define MAX_MQTTTOP_LENGTH         (128U)          


typedef struct 
{
    unsigned char type;
    unsigned char subTopEnd[TOPIC_END_NUM];
}SUBTOPIC_END_T;


//物模型主题
static SUBTOPIC_END_T subModelTopic[MQTT_MODEL_TOP_MAX] = 
{
    {MQTT_PRO_POST_REPLY,       "thing/property/post_reply"},
    {MQTT_PRO_SET,              "thing/property/set"},
    {MQTT_PRO_READ,             "thing/property/read"},
    {MQTT_EVENT_POST_REPLY,     "thing/event/+/post_reply"},
    {MQTT_SERVICE_INVOKE,       "thing/service/+/invoke"},
    {MQTT_UPGRADE_OTA_URL,      "ota/device/upgrade"},
    {MQTT_PARA_DOWN_RAW,        "ctlb/model/down_raw"},
    {MQTT_PARA_UP_RAW_REPLY,    "ctlb/model/up_raw_reply"},
    {MQTT_THING_DEV_INFOR_REPLY,"thing/deviceinfo_reply"},
    {MQT_RAW_DOWN,               "raw/down"},
};


MQTT_CLIENT_T mqttClient;
static mqtt_sta_callback   mqttCallback;

/**
 * @description: 操作消息ID
 * 
 * @param [in]   op  : 操作方法
 * 
 * @return {*}
 */
static unsigned short lyy_mqtt_messageId(unsigned int op)
{

#define INIT_PACKET_ID         (1U)
#define GET_PACKE_ID           (2U)
#define ADD_PACKE_ID           (3U)

    static unsigned short packetid;

    return packetid = (op == INIT_PACKET_ID) ? INIT_PACKET_ID : 
                                            (op == GET_PACKE_ID ? (uint32_t)packetid : (uint32_t)(packetid + 1));
}

/**
 * @description: 物模型主题订阅
 * 
 * @param [in]   topicEnd
 * @param [in]   message
 * 
 * @return {*}
 */
static void lyy_mqtt_model_subscribe(const char *topicEnd, unsigned short message)
{

    char subTopicName[MAX_MQTTTOP_LENGTH]  = {0x00};

    sprintf(subTopicName,"sys/%s/%s/%s",    APP_pcGetProductKey(),
                                            APP_pcGetParamDevId(),
                                            topicEnd);

    api_mqtt_subscribe(subTopicName,message);

}


/**
 * @brief 
 * 发布信息
 * @param topic 
 * @param data 
 * @param dataLen 
 */
void lyy_mqtt_model_publish(const char *topic, unsigned char *data, unsigned int dataLen)
{
    
    LYY_LOGI("lyy_mqtt_model_publish: %s:%s",topic,data);

    if(lyy_mqtt_get_status())
        api_mqtt_pubish(topic,(const char *)data,dataLen);

}


/**
 * @description: 数据报文处理
 * 
 * @param [in]    payload
 * @param [in]    topicInfo
 * 
 * @return {*}
 */
static void lyy_mqtt_pub_recv(MQTT_PAYLOAD_T *payload, MQTT_TOPIC_INFO_T *topicInfo)
{

    char topicChk[128] = {0x00,};
    char *chk = NULL;

    if(NULL == topicInfo->topic && NULL == payload)
    {
        return ;
    }

    memset(topicChk, 0, sizeof(topicChk));


    sprintf(topicChk ,"sys/%s/%s/", APP_pcGetProductKey(),
                                    APP_pcGetParamDevId());

    //校验topic是否为本机
    chk = strstr((const char*)topicInfo->topic, topicChk);
    if(chk == NULL)
    {
        LYY_LOGI("topic chk fail!!!");
        return;
    }

    //payload处理-没有加密
    chk += strlen(topicChk);  //那后面不一样部分

    lyy_thing_down_parse((char *)payload->payload, (uint32_t)payload->payloadLen, chk);

}


/**
 * @description: 监控心跳
 * 
 * @param [in]    mqttHeart
 * 
 * @return {*}
 */
void lyy_mqtt_monitor_heart(enum mqtt_heart mqttHeart)
{
    #define LMQ_HEART_WAIT_TIME                       (10U)        /* 6分钟没收到心跳回复重启设备 */

    static unsigned short heartCnt;

    switch(mqttHeart)
    {
        case MQTT_HEART_RECV_SUCCESS :
        {
            heartCnt = 0x00;
        } break;

        case MQTT_WAIT_HEART         :
        {

            if(++heartCnt >= LMQ_HEART_WAIT_TIME)
            {
                HAL_vReset("### heart wait timeout", 1000);
            }

            LYY_LOGI("lmq heart wait cnt: [%d]", heartCnt);

        } break;

        default : break;
    }
}




void lyy_mqtt_connect(void)
{

#define MQTT_KEEPALIVE                            (40U)       /* 消息间隔 */

    MQTT_INFOR_DEF xMqttInfoDef = {0x00, };

    lyy_mqtt_param_get(&xMqttInfoDef);

    api_mqtt_connect(   xMqttInfoDef.username,
                        xMqttInfoDef.clientId,
                        xMqttInfoDef.password,
                        MQTT_KEEPALIVE);
    
}


/**
 * @description: mqtt yield
 * 
 * @param [in]   arg        回调形参
 * 
 * @return {*}
 */
void lyy_mqtt_status_task(void *arg)
{

    (void)arg;

    uint8_t ret = 0;
    static unsigned short   subTop  = 0;
    static uint8_t          gap     = 0;
    static uint8_t          cnt     = 0;

    switch(mqttClient.clientSta)
    {

        case MQTT_STATE_INIT:
        {

            LYY_LOGI("MQTT_STATE_INIT");
        
            lyy_mqtt_messageId(INIT_PACKET_ID);

            mqttClient.connect_flag = 0x00;
            mqttClient.clientSta    = MQTT_STATE_CONNECT;

        }break;

        case MQTT_STATE_CONNECT: 
        {

            cnt++;
            gap++;
            gap %= 7; //最多1分钟间隔
           
            LYY_LOGI("mqtt connect cnt : %d,gap: %d",cnt,gap);   

            lyy_mqtt_connect();

            mqttClient.ucSemSta = 0x01;
            ret = xSemaphoreTake(mqttClient.sem, 10000 * gap);  //10秒间隔 ，递增到30分钟重启模块
            mqttClient.ucSemSta = 0x00;

            if(ret > 0)//成功
            {
                mqttClient.clientSta    = MQTT_STATE_SUBSCRIBE;  
                mqttClient.connect_flag = 0x01; //链接成功
                gap                     = 0; 
                cnt                     = 0;
                subTop                  = 0;
            }
            else
            {
                if(cnt>36) //大于30分钟复位
                {
                    HAL_vReset("mqtt connect timeout to reset system", 1000);
                }
            }

        } break;


        case MQTT_STATE_SUBSCRIBE: 
        {

            LYY_LOGI(">>> subTopEnd : [%s]   linkCnt : [%d]", subModelTopic[subTop].subTopEnd, cnt);
            cnt++;
            gap++;
            gap %= 7; //最多1分钟间隔

            LYY_LOGI("mqtt connect cnt : %d",cnt); 

            lyy_mqtt_model_subscribe((char *)subModelTopic[subTop].subTopEnd,lyy_mqtt_messageId(GET_PACKE_ID));

            mqttClient.ucSemSta = 0x01;
            ret = xSemaphoreTake(mqttClient.sem, 10000 * gap);  //10秒间隔 ，递增到30分钟重启模块
            mqttClient.ucSemSta = 0x00;

            if(ret > 0)//成功
            {
                gap = 0; 
                cnt = 0;
                lyy_mqtt_messageId(ADD_PACKE_ID); //刷新packid
                subTop++;
                if(subTop >= sizeof(subModelTopic)/sizeof(subModelTopic[0])) //
                {
                    mqttClient.clientSta = MQTT_STATE_SUCCESS;  
                }
            }
            else
            {
                if(cnt>36) //大于30分钟复位
                {
                    HAL_vReset("mqtt subscribe timeout to reset system", 1000);
                }
            }

        } break;

        case MQTT_STATE_DISCONNECTED: 
        {
            mqttClient.clientSta = MQTT_STATE_STANDBY;
        } break;

        case MQTT_STATE_SUCCESS:  //成功(跑支付队列)
        { 
            //todo..
        }break;

        case MQTT_STATE_ISPAUSED:
            break;
        default : break;

    }

    if(mqttCallback)
    {
        mqttCallback(mqttClient.clientSta);
    }

}

void lyy_mqtt_sta_proc(mqtt_sta_callback func)
{
    mqttCallback = func;
}

/**
 * @brief 
 * 获取mqtt 状态
 * @return int 
 * TRUE 链接正常
 * FALSE 失败
 */
int lyy_mqtt_get_status(void)
{
    return mqttClient.clientSta == MQTT_STATE_SUCCESS ? 0x01 : 0x00;
}

/**
 * @brief 
 * 获取mqtt 阶段
 * @return int 
 */
int lyy_mqtt_get_step(void)
{
   return mqttClient.clientSta; 
}

/**
 * @brief 
 * 获取mqtt 状态
 * @return int 
 * TRUE 链接正常
 * FALSE 失败
 */
void lyy_mqtt_set_status(uint8_t status)
{
    mqttClient.clientSta = status;
}

/**
 * @brief 直接设置 MQTT 客户端处理阶段。
 * @param[in] para 阶段值，赋值到 uint8_t clientSta，不作范围校验。
 * @note MQTT_STATE_ISPAUSED 阶段不推进状态机，但任务仍会调用已注册的状态回调。
 */
void lyy_mqtt_set_step(int para)
{
    mqttClient.clientSta = para;
}

/**
 * @brief 
 * 获取链接状态
 * @return int 
 * TRUE  : 成功
 * FALSE : 失败
 */
int lyy_mqtt_connect_result(void)
{
    return mqttClient.connect_flag;
}

/**
 * @description: mqtt开始连接
 * @param {*}
 * @return {*}
 */
int lyy_mqtt_start(void)
{
    mqttClient.clientSta = MQTT_STATE_INIT;

    return LYY_LMQ_SUCCESS;
}



/**
 * @description: mqtt重连
 * @param {*}
 * @return {*}
 */
int lyy_mqtt_reconnect(void)
{
    return lyy_mqtt_start();
}


/**
 * @description: mqtt断开连接
 * @param {*}
 * @return {*}
 */
void lyy_mqtt_disconnected(void)
{
    mqttClient.clientSta = MQTT_STATE_DISCONNECTED;
}


/**
 * @description: 校验数据
 * @param {*}
 * @return {*}
 */
signed char lyy_mqtt_isPacket(unsigned char *buf, unsigned short bufLen)
{
    /* 校验数据失败 */
    if(NULL == buf || bufLen < MIN_MQTT_LEN)
    {
        return LYY_LMQ_FAIL;
    }

    int mqttType = mqtt_contorlDecode(buf, bufLen);

    /* 存在叠包情况 */
    if((mqttType == MQTT_FAIL || mqttType < bufLen)) 
    {
        mqttType = 0;
    }
    else 
    {
        mqttType = 1;
    }

    return mqttType;
}


/**
 * @description: 叠包处理
 * 
 * @param [in]   buf
 * @param [in]   bufLen
 * @param [in]   parse  
 * 
 * @return {*}
 */
signed char lyy_mqtt_overlap(unsigned char *buf, unsigned short bufLen, int (*parse)(unsigned char *, unsigned short))
{
    int myLen            = 0;
    unsigned short i     = 0;
    
    /* mqtt数据固定报头最少2 byte */
    for( ; (unsigned int)(bufLen - i) >= MIN_MQTT_LEN; i++)
    {

        myLen = mqtt_contorlDecode(buf+i, bufLen-i);

        if(myLen != MQTT_FAIL && myLen <= (bufLen - i))
        {
            parse(buf + i, myLen);
            i += (myLen - 1);
        }

    }

    return 1;
} 


/**
 * @description: 数据包解析
 * 
 * @param [in]   buf
 * @param [in]   bufLen
 * 
 * @return {*}
 */
int lyy_mqtt_packet_parse(unsigned char *buf, unsigned short bufLen)
{

    int               ret           = 0;
    int               recvCode      = 0;
    int               mqttConCode;
    unsigned short    messageId;
    MQTT_PAYLOAD_T    payload;
    MQTT_TOPIC_INFO_T topicInfo;

    topicInfo.topic = malloc(MAX_MQTTTOP_LENGTH);
    
    if(NULL == topicInfo.topic)
    {
        return ret;
    }

    memset(topicInfo.topic, 0x00, MAX_MQTTTOP_LENGTH);

    mqttConCode = mqtt_dataParse(buf, bufLen, &recvCode, &messageId, &payload, &topicInfo);

    if(mqttConCode == MQTT_FAIL)
    {
        goto lmq_pack_exit;
    }

    switch(mqttConCode)
    {

        case MQTT_CONNACK  :
        {
            LYY_LOGI("mqtt connect ack");

            if(mqttClient.ucSemSta)
            {
                xSemaphoreGive(mqttClient.sem);
                mqttClient.ucSemSta = 0x00;
            }

        }break;

        case MQTT_SUBACK   :
        {
            LYY_LOGI("mqtt sub ack");

            if(recvCode == LYY_LMQ_QOS && messageId == lyy_mqtt_messageId(GET_PACKE_ID))
            {
                if(mqttClient.ucSemSta)
                {
                    xSemaphoreGive(mqttClient.sem);
                    mqttClient.ucSemSta = 0x00;
                }
            }

        }break; 

        case MQTT_PUBLISH  :
        {

            LYY_LOGI("mqtt publish");
            lyy_mqtt_pub_recv(&payload, &topicInfo);

        }break;

        case MQTT_PINGRESP :
        {

            LYY_LOGI("mqtt ping ack");
            /* 应用层维护心跳 */
            lyy_mqtt_monitor_heart(MQTT_HEART_RECV_SUCCESS);
        }

        case MQTT_UNSUBACK :break;

        default: break;
    }

    ret = 1;
    
lmq_pack_exit :

    free(topicInfo.topic);

    return ret;
}



void lyy_mqtt_thing_init(void)
{

    mqttClient.sem      = xSemaphoreCreateBinary();
    mqttClient.ucSemSta = 0x00;

}

