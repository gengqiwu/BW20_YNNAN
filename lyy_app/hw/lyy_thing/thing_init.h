/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2022-05-27 17:33:49
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-08-06 21:22:20
 * @FilePath: \lyy_std_sdk\system\lmq\inc\lyy_lmq_app.h
 */
#ifndef __THING_INIT_H__
#define __THING_INIT_H__


#define THING_OTA_DEFAULT                   "default"
#define MODEL_SIGN_METHOD                   "hmacsha256"


enum{

    MQTT_STATUS_START,
    MQTT_STATUS_TICK,
};


typedef struct
{
    char username[64];
    char password[128];
    char clientId[128];    

}__attribute__((aligned(1), packed))MQTT_INFOR_DEF;


typedef struct
{
    uint8_t mqtt_status;   
}__attribute__((aligned(1), packed)) MQTT_STATUS_DEF;


extern void vRandomGet(char *cIp, uint32_t ulLen);

extern void lyy_mqtt_model_init(void);
extern void lyy_mqtt_paramInit(char *devid);
extern void lyy_mqtt_param_get(MQTT_INFOR_DEF *pxMqttInfo);

extern void box_post_cloud_device_infor(void);

#endif