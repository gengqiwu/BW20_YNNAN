/*
 * @Author: your name
 * @Date: 2022-01-20 17:55:18
 * @LastEditTime: 2024-12-06 21:11:22
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @FilePath: \lyy_sdk_mqtt\app\src\mqtt_client.c
 */
#include <string.h>
#include <stdint.h>

#include "mqtt_api.h"
#include "MQTTPacket.h"

#include "HAL_Log.h"
#include "HAL_System.h"
#include "APP_NetWork.h"

/* 
*  代码根据LMQ协议内容封装，应用层关于QOS1与QOS2等相关的运行流程未封装，尽量使得代码精简。 
*  1.乐摇摇LMQ采用3.1.1版本MQTT，版本标识为4
*  2.订阅和取消订阅每次只进行一个主题（接口封装）
*/

#define MQTT_DATA_PARSING_SUCCESS         (1U)

/**
 * @description:  mqtt socket 数据发送
 * 
 * @param [in] buf  数据包
 * @param [in] len  数据包大小
 * 
 * @return {*}
 */
static int api_mqtt_send(unsigned char *buf, unsigned int len)
{

    if(NULL == buf || 0x00 == len)
    {
        return 0x00;
    }

    return APP_lClientWrite((char *)buf, len);

}


/**
 * @brief 
 * 连接mqtt
 * @param username 
 * @param clientid 
 * @param password 
 * @param keeplive 
 * @return int 
 */
int api_mqtt_connect(char *username, char *clientid, char *password, uint16_t keepalive)
{  

#define MQTT_CONNECT_CLEAN_SESSION      1  //清除会话  0 不清空，1 清空
#define MQTT_CONNECT_WILL_FLAG          0  //遗嘱信息  0 不启用，1 启用
#define MQTT_CONNECT_VERSION            4  //mqtt协议版本
#define MQTT_CONNECT_BUFFER_LEN_MAX     1024 

    int     len;
    uint8_t connect_body[MQTT_CONNECT_BUFFER_LEN_MAX] = {0};
    
    /* 初始化报文(mqtt为3.1.1版本、默认保持60s的间隔通讯、默认清理会话) */
    MQTTPacket_connectData mqttConnectData = MQTTPacket_connectData_initializer;
    
    /* keepalive/cleansession */
    mqttConnectData.keepAliveInterval   = keepalive;
    mqttConnectData.willFlag            = MQTT_CONNECT_WILL_FLAG;
    mqttConnectData.cleansession        = MQTT_CONNECT_CLEAN_SESSION;
    mqttConnectData.MQTTVersion         = 4;

    mqttConnectData.clientID.cstring = clientid;
    mqttConnectData.username.cstring = username;
    mqttConnectData.password.cstring = password;

    len = MQTTSerialize_connect(connect_body, MQTT_CONNECT_BUFFER_LEN_MAX, &mqttConnectData);

    return api_mqtt_send(connect_body, len);

}


/**
 * @brief 
 * 发布信息
 * @param topic 
 * @param payload 
 * @param len 
 * @return int 
 */
int api_mqtt_pubish(const char *topic, const char *payload, uint16_t len)
{

#define _MQTT_DUP           0
#define _MQTT_QOS           0
#define _MQTT_RETAINED      0
#define _MQTT_PACKID        0


    if(NULL == payload || topic ==NULL)
        return -1;

    //uint8_t buffer[2048]    =   {0x00, }; 

    uint8_t *pucBuff = NULL;
    pucBuff = pvPortMalloc(2048);

    if(!pucBuff) {

        LYY_LOGE("PUBLSH pucBuff MALLOC ERR");
        return 0x00;
    }

    memset(pucBuff, 0x00, 2048);

    uint16_t data_len;
    MQTTString topic_name   = MQTTString_initializer;
    topic_name.cstring      = (char *)topic;

    data_len = MQTTSerialize_publish(   pucBuff, 
                                        2048, 
                                        _MQTT_DUP, 
                                        _MQTT_QOS, 
                                        _MQTT_RETAINED, 
                                        _MQTT_PACKID, 
                                        topic_name, 
                                        (uint8_t *)payload, 
                                        len);

    
    int lRet = api_mqtt_send(pucBuff, data_len);

    vPortFree(pucBuff);

    return lRet;

}

/**
 * @brief 
 * 订阅
 * @param topic 
 * @param messageID 
 * @return int 
 */
int api_mqtt_subscribe(char *topic,uint16_t messageID)
{

#define _MQTT_SUBSCRIBE_DUP             0
#define _MQTT_SUBSCRIBE_QOS             0
#define _MQTT_SUBSCRIBE_NUMBER          1
#define _MQTT_SUBSCRIBE_LEN_MAX         1024

    if(topic == NULL)
        return -1;
        
    uint8_t     buffer[_MQTT_SUBSCRIBE_LEN_MAX]  = {0}; 
    uint16_t    data_len;

    int qos                 = _MQTT_SUBSCRIBE_QOS;
    MQTTString topic_name   = MQTTString_initializer;
    topic_name.cstring      = topic;

    data_len = MQTTSerialize_subscribe(buffer, 
                                   _MQTT_SUBSCRIBE_LEN_MAX, 
                                   _MQTT_SUBSCRIBE_DUP, 
                                   messageID, 
                                   _MQTT_SUBSCRIBE_NUMBER , 
                                   &topic_name, 
                                   &qos);

    return api_mqtt_send(buffer, data_len);

}


/**
 * @brief 
 * 心跳
 * @param pack 
 * @param len 
 */
int api_mqtt_tick(void)
{

    uint8_t     pack[512];
    uint16_t    data_len = 0;

    data_len = MQTTSerialize_pingreq(pack, 512);

    return api_mqtt_send(pack, data_len);

}


/**
 * @description: mqtt控制报文的类型
 * 
 * @param [in]  buf   : mqtt协议控制报文字节
 * 
 * @return 返回mqtt控制报文的类型
 */
static int mqtt_heard_type(unsigned char buf)
{
    MQTTHeader header = {0x00};
    header.byte       = buf;

	return header.bits.type;
}


/**
 * @description: mqtt控制报文类型解析
 * 
 * @param [in]   buf        : 接收server数据
 * @param [in]   bufLen     : 接收server数据的长度
 * 
 * @return {*}   数据帧长度
 */
int mqtt_contorlDecode(unsigned char *buf, unsigned short bufLen)
{
                 
    #define MQTT_CONTORL_FIELD_LEN         (1U)                  

    if(NULL == buf || bufLen < MIN_MQTT_LEN)
        return MQTT_FAIL;

    int heardLen, myLen, controlHeard;                                                       

    heardLen      = MQTT_CONTORL_FIELD_LEN;
    heardLen     += MQTTPacket_decodeBuf((buf + MQTT_CONTORL_FIELD_LEN), &myLen);     
    controlHeard  = mqtt_heard_type(buf[0]);

    if(((myLen + heardLen) > bufLen) || (controlHeard < CONNECT
                                            || DISCONNECT < controlHeard))
    {
        return MQTT_FAIL;
    }

    return (myLen + heardLen);

}



/**
 * @description: mqtt CONACK 报文解析
 * 
 * @param [in]   buf        : 接收server数据
 * @param [in]   bufLen     : 接收server数据的长度
 * 
 * @return {*}
 */
static int mqtt_recv_connAck(unsigned char *buf, unsigned short bufLen)
{
    if(NULL == buf)
        return MQTT_FAIL;


    int ret = MQTT_FAIL;
    unsigned char connRecvCode, sessionPresent;

    if (MQTT_DATA_PARSING_SUCCESS == MQTTDeserialize_connack(&sessionPresent, 
                                                             &connRecvCode, 
                                                             buf, 
                                                             (int)bufLen)) 
    {
        ret = connRecvCode;
    }

    return ret;
}


/**
 * @description: mqtt SUBACK 报文解析
 * 
 * @param [in]   buf        : 接收server数据
 * @param [in]   bufLen     : 接收server数据的长度
 * @param [in]   messageId  : 消息id
 * 
 * @return {*}
 */
static int mqtt_recv_subAck(unsigned char *buf, unsigned short bufLen, unsigned short *messageId)
{

    if(NULL == buf)
        return MQTT_FAIL;


/* 接口设置每次只订阅一个主题 */
#define SUBSCIRBE_MAX              (1U)            

    int grantedQos;
    int count = 0;
    int ret   = MQTT_FAIL;

    if(MQTT_DATA_PARSING_SUCCESS == MQTTDeserialize_suback(messageId, 
                                                           SUBSCIRBE_MAX, 
                                                           &count, 
                                                           &grantedQos, 
                                                           buf, 
                                                           (int)bufLen)) 
    {
        ret = grantedQos;
    }

    return ret;
}


/**
 * @description: 接收到推送信息
 * 
 * @param [in]   buf        : 接收server数据
 * @param [in]   bufLen     : 接收server数据的长度
 * @param [out]  payload    : 存储负荷内容
 * 
 * @return {*}
 */
static int mqtt_recv_publish(unsigned char *buf, unsigned short bufLen, MQTT_PAYLOAD_T *payload, MQTT_TOPIC_INFO_T *topicInfo)
{
    if(NULL == buf)
        return MQTT_FAIL;


    MQTTString  topicName       = MQTTString_initializer;

    if (MQTT_DATA_PARSING_SUCCESS != MQTTDeserialize_publish(&topicInfo->dup,
                                                             &topicInfo->qos,
                                                             &topicInfo->retained,
                                                             &topicInfo->packetId,
                                                             &topicName,
                                                             &payload->payload,
                                                             &payload->payloadLen,
                                                             buf,
                                                             bufLen)) 
    {
        return MQTT_FAIL;
    }


    memcpy((unsigned char *)topicInfo->topic, (const unsigned char *)topicName.lenstring.data, topicName.lenstring.len);

    return MQTT_SUCCESS;

}


/**
 * @description: mqtt报文解析(裁剪版----> QOS0)
 * 
 * @param [in]   buf        : 接收server数据
 * @param [in]   bufLen     : 接收server数据的长度
 * @param [in]   messageId  : 消息ID的获取
 * @param [in]   recvCode   : 响应码
 * @param [in]   payload    : 存储解析出来的负荷payload数据
 * 
 * @return {*}
 */
int mqtt_dataParse(unsigned char *buf, unsigned short bufLen, int *recvCode, unsigned short *messageId, MQTT_PAYLOAD_T *payload, MQTT_TOPIC_INFO_T *topicInfo)
{
    int mqttType = mqtt_heard_type(buf[0]);

    switch(mqttType)
    {

        case CONNACK  :
        {
            *recvCode = mqtt_recv_connAck(buf, bufLen);
            mqttType = MQTT_CONNACK;

        } break;

        case SUBACK   :
        {
            *recvCode = mqtt_recv_subAck(buf, bufLen, messageId);
            mqttType = MQTT_SUBACK;

        } break; 

        case PUBLISH  :
        {
            *recvCode = mqtt_recv_publish(buf, bufLen, payload, topicInfo);
            mqttType = MQTT_PUBLISH;

        } break;

        case PINGRESP :
        {
            mqttType = MQTT_PINGRESP; 

        } break;

        case UNSUBACK :
        {
            mqttType = MQTT_UNSUBACK; 

        } break;
        
        default : break;
    }   

    return mqttType;
}


