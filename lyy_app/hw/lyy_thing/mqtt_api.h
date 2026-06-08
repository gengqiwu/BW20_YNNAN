/*
 * @Author: your name
 * @Date: 2022-01-20 17:55:09
 * @LastEditTime: 2024-07-31 14:35:25
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @FilePath: \sdk_mqtt\app\inc\mqtt_client.h
 */
#ifndef  __MQTT_API_H__
#define  __MQTT_API_H__


#define MQTT_SUCCESS             (0)
#define MQTT_FAIL                (-1)

#define MIN_MQTT_LEN             (2U) 
#define MAX_MQTTCFG_LENGTH       (64U)


#pragma pack(push, 1)


/* 遗嘱信息 */
typedef struct 
{
    char          qos;
	unsigned char retained;
    char          topicName[MAX_MQTTCFG_LENGTH];
	char          message[MAX_MQTTCFG_LENGTH * 2];
}MQTT_WILL_MES;


/* mqtt client 连接信息 */
typedef struct 
{
    unsigned char    cleansession;                  /* 清理会话 */
    unsigned char    willFlag;                      /* 遗嘱标识 */
    unsigned short   keepalive;                     /* 数据交互间隔 */
    MQTT_WILL_MES    mqttWill;                      /* 遗嘱信息 */
    char clientID[MAX_MQTTCFG_LENGTH];              /* 客户端标识 */
    char username[MAX_MQTTCFG_LENGTH];              /* 用户名称 */
    char password[MAX_MQTTCFG_LENGTH];              /* 密码 */
}MQTT_CONNECT_INFO;


typedef struct 
{
    unsigned char  dup;
    unsigned char  retained;
    unsigned short packetId;
    int            qos; 
    unsigned char  *topic;                     
}MQTT_TOPIC_INFO_T;


/* mqtt payload */
typedef struct
{
    unsigned char *payload;                         /* 负荷内容 */
    int            payloadLen;                      /* 负荷内容的长度 */
}MQTT_PAYLOAD_T;


/* mqtt 封装数据 */
typedef struct 
{
    unsigned char *buf;                             /* 存储封装MQTT报文的数据 */
    unsigned int   bufLen;                          /* buf的大小 */
}MQTT_MESSAGE_T;


enum mqtt_msgType 
{
    MQTT_CONNACK,
    MQTT_SUBACK,
    MQTT_PUBLISH,
    MQTT_UNSUBACK,
    MQTT_PINGRESP,
    MQTT_DISCONNECT,
};

/* mqtt连接返回码(connect) */
enum mqtt_connect_recv_code 
{
    MQTT_CONNECTED,                                 /* 连接成功 */
    MQTT_NO_SUPPORT_AGREE,                          /* 连接已拒绝，不支持协议版本 */
    MQTT_CLIENT_LOGO_FAIL,                          /* 连接已拒绝，客户端标识符不合格，其中clientID为空，clienSeeion为0，也会报此错误  */
    MQTT_BORKER_FAIL,                               /* socket已连接，但MQTT代理服务器不可用 */
    MQTT_USER_MESSAGE_FAIL,                         /* 用户名与密码错误(当服务器筛选信息的情况下) */
    MQTT_CLIENT_UNAUTHORIZED,                       /* 客户端未被授权连接到此服务器 */
};


/* mqtt质量服务等级 */
enum mqtt_qos_code 
{
    LYY_MQTT_QOS0,
    LYY_MQTT_QOS1,
    LYY_MQTT_QOS2,
    LYY_MQTT_SUB_FAIL = 0x80,                           /* MQTT订阅失败 */
};

#pragma pack(pop)


extern int mqtt_heart(MQTT_MESSAGE_T *clientMess);
extern int mqtt_disconnect(MQTT_MESSAGE_T *clientMess);
extern int mqtt_contorlDecode(unsigned char *buf, unsigned short bufLen);
extern int mqtt_connect(MQTT_MESSAGE_T *clientMess, MQTT_CONNECT_INFO *connectInfo);
extern int mqtt_unsubscribe(MQTT_MESSAGE_T *clientMess, const char *subTopicName, unsigned char dup);
extern int mqtt_publish(MQTT_MESSAGE_T *clientMess, MQTT_PAYLOAD_T *pubMessage, MQTT_TOPIC_INFO_T *topicInfo);
extern int mqtt_subscribe(MQTT_MESSAGE_T *clientMess, const char *subTopicName, unsigned short messageId, unsigned char dup, int qos);
extern int mqtt_dataParse(unsigned char *buf, unsigned short bufLen, int *recvCode, unsigned short *messageId, MQTT_PAYLOAD_T *payload, MQTT_TOPIC_INFO_T *topicInfo);

//外部使用接口
extern int api_mqtt_tick(void);
extern int api_mqtt_subscribe(char *topic,uint16_t messageID);
extern int api_mqtt_pubish(const char *topic,const char *payload,uint16_t len);
extern int api_mqtt_connect(char *username,char *clientid,char *password,uint16_t keepalive);

#endif
