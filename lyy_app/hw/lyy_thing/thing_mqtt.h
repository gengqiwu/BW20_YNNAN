/*
 * @Author: your name
 * @Date: 2022-01-24 17:42:00
 * @LastEditTime: 2024-08-06 21:28:31
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @FilePath: \lyy_app_wwj\lyy_app\user\lyy_v1\lmq\inc\lyy_lmq_client.h
 */
#ifndef __THING_MQTT_H__
#define __THING_MQTT_H__

#include "HAL_System.h"

#define LYY_LMQ_SUCCESS                 (0)
#define LYY_LMQ_FAIL                    (-1)

#define PACKED                          __attribute__((packed))

typedef enum 
{
    MQTT_STATE_STANDBY,                 /* 空闲状态 */
    MQTT_STATE_INIT,
    MQTT_STATE_CONNECT,                 /* MQTT启动连接 */
    MQTT_STATE_SUBSCRIBE,               /* MQTT启动订阅 */
    MQTT_STATE_DISCONNECTED,            /* MQTT处于断开连接状态 */
    MQTT_STATE_SUCCESS,
}PACKED MQTT_CLIENT_STATE_T;


enum mqtt_topic 
{
    MQTT_DOWM_STREAM,
    MQTT_UPGRADE,
    MQTT_PASS_THROUGH,
    MQTT_UP_STREAM,
};

//物模型主题
enum mqtt_model_topic 
{
    MQTT_PRO_POST_REPLY,            //属性上报应答
    MQTT_PRO_SET,                   //属性设置
    MQTT_PRO_READ,                  //属性读取
    MQTT_EVENT_POST_REPLY,          //事件上报应答
    MQTT_SERVICE_INVOKE,            //服务
    MQTT_UPGRADE_OTA_URL,           //ota-url链接下载
    MQTT_PARA_DOWN_RAW,             //云端下发raw
    MQTT_PARA_UP_RAW_REPLY,         //上行raw应答
    MQTT_THING_DEV_INFOR_REPLY,     //设备标签应答
    MQT_RAW_DOWN,                   //反向下发
    MQTT_MODEL_TOP_MAX
};

enum mqtt_heart 
{
    MQTT_WAIT_HEART,
    MQTT_HEART_RECV_SUCCESS
};


typedef struct 
{
    uint8_t              clientSta;
    uint8_t              connect_flag;
    uint8_t              ucSemSta;
    SemaphoreHandle_t    sem;
}__attribute__((aligned(1), packed)) MQTT_CLIENT_T;


typedef void (*mqtt_sta_callback)(MQTT_CLIENT_STATE_T);

extern MQTT_CLIENT_T mqttClient;

/* mqtt 相关操作 */
extern int  lyy_mqtt_start(void);
extern int  lyy_mqtt_reconnect(void);
extern void lyy_mqtt_disconnected(void);
extern void lyy_mqtt_status_task(void *arg);
extern void lyy_mqtt_set_status(uint8_t status);

extern int  lyy_mqtt_get_step(void);
extern int  lyy_mqtt_get_status(void);
extern int  lyy_mqtt_connect_result(void);

/* 数据接收处理接口 */
extern int         lyy_mqtt_packet_parse(unsigned char *buf,  unsigned short bufLen);
extern signed char lyy_mqtt_isPacket(unsigned char *buf,      unsigned short bufLen);
extern signed char lyy_mqtt_overlap(unsigned char *buf,       unsigned short bufLen, int (*parse)(unsigned char *, unsigned short));

/* 海外物模型相关 */
extern void lyy_mqtt_model_publish(const char *topic, unsigned char *data, unsigned int dataLen);

extern void lyy_mqtt_thing_init(void);
extern void lyy_mqtt_monitor_heart(enum mqtt_heart mqttHeart);
extern void lyy_mqtt_sta_proc(mqtt_sta_callback func);

#endif