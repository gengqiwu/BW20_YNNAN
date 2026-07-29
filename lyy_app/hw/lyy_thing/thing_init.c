/*
 * @Author: e.he
 * @Date: 2022-05-27 17:33:56
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 18:25:49
 * @FilePath:
 */
#include "stdio.h"
#include "stdint.h"
#include "string.h"

#include "hmac_sha2.h"

#include "HAL_Log.h"
#include "HAL_System.h"

#include "APP_Led.h"
#include "APP_Param.h"


#include "lyy_pic.h"
#include "lyy_para.h"
#include "lyy_device.h"

#include "mqtt_api.h"
#include "thing_iot.h"
#include "thing_init.h"
#include "thing_mqtt.h"
#include "thing_event.h"
#include "thing_property.h"


#include "utils.h"

#include "USER_Config.h"




enum {

#ifdef CFG_HEAP_DEBUG
    eHeapMonitorTask,
#endif

    eMqttHeartTask,
    eMqttCsqTask,
    eTaskMax,
};


enum device_lmq_sta
{
    LMQ_IDE,
    LMQ_LOGINING,
    LMQ_LOGIN_OK,
    LMQ_LOGIN_FAIL,
    LMQ_CONNECTED,
};



static struct HalSoftTimer g_AppTask[eTaskMax];


#ifdef CFG_HEAP_DEBUG


static void APP_prvHeapMonitorTask(void *pxArg)
{

    (void)pxArg;

    LYY_LOGI("Free heap: [%d] bytes",  (uint32_t)xPortGetFreeHeapSize());

}


#endif


static MQTT_INFOR_DEF mqtt_infor_t;

/**
 * @brief 
 * 获取
 * @param username 
 * @param password 
 * @param clientId 
 * @return int 
 */
void lyy_mqtt_param_get(MQTT_INFOR_DEF *pxMqttInfo)
{
    memcpy(pxMqttInfo, &mqtt_infor_t, sizeof(MQTT_INFOR_DEF));
}


/**
 * @description: lmq参数打印
 * @return {*}
 */
static void lyy_mqtt_param_printf(void)
{

    LYY_LOGI("iot_productKey: %s", APP_pcGetProductKey());
    LYY_LOGI("iot_productSecret: %s", APP_pcGetProductSecret());    
    LYY_LOGI("username: %s", mqtt_infor_t.username); 
    LYY_LOGI("clientId: %s", mqtt_infor_t.clientId);
    LYY_LOGI("password: %s", mqtt_infor_t.password); 

}


/**
 * @description: lmq参数生成
 * @return {*}
 */
void lyy_mqtt_paramInit(char *devid)
{

    if(NULL == devid) {

        return ;
    }

    char random[]           = "lyy_iot";        //随机数，暂时固定

    char proSecret[30]      = {0};
    char content[64]        = {0x00,};
    char passwordBuff[32]   = {0x00,};

    //clientId	客户端ID，可自定义，长度在64个字符内。建议使用设备的MAC地址或SN码，方便您识别区分不同的客户端。
    sprintf((char *)mqtt_infor_t.clientId,"%s|random=%s,signmethod=%s|", devid, random, MODEL_SIGN_METHOD);
    sprintf((char *)mqtt_infor_t.username,"%s&%s", devid, APP_pcGetProductKey());


    memset(content, 0, sizeof(content));
    sprintf(content, "random=%s,signmethod=%s", random, MODEL_SIGN_METHOD);

    memset(passwordBuff, 0, sizeof(passwordBuff));
    strcpy(proSecret,APP_pcGetProductSecret());


    hmac_sha256_esp((unsigned char *)proSecret, strlen(proSecret), 
                    (unsigned char *)content,   strlen(content), 
                    (unsigned char *)passwordBuff, sizeof(passwordBuff));

    utils_lHexToStringLower((char *)mqtt_infor_t.password, (const unsigned char*)passwordBuff, sizeof(passwordBuff));//要用小写的

    lyy_mqtt_param_printf();

}




/**
 * @brief 
 * 获取云端支付参数
 */
void box_get_cloud_pay_param(void)
{

    event_post_query_pay_channels(); //支付渠道
    vTaskDelay(500  / portTICK_PERIOD_MS);

    //ndj_add
    if(get_dev_type_code() == DEVICE_TYPE_UART_NDJ) //扭蛋机
    {
        event_post_query_lane_info_post();
        vTaskDelay(1000  / portTICK_PERIOD_MS);
        event_post_query_pay_packages_ndj("CONSUMPTION",1,0,"true");
        vTaskDelay(1000  / portTICK_PERIOD_MS);
    }
    else if (get_dev_type_code() == DEVICE_TYPE_UART_SHJ || get_dev_type_code() == DEVICE_TYPE_UART_YUNANSHJ)  //售货机不获取套餐
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        event_post_query_position_info(1, 12, false, false);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }   
    else  //其他都是一样的方式
    {
        event_post_query_pay_packages("CONSUMPTION");
        vTaskDelay(1000  / portTICK_PERIOD_MS);
    } 


    event_post_query_business_infor(); //区域信息
    
    vTaskDelay(1000  / portTICK_PERIOD_MS);
    event_post_err_message("en-US",login_mark_get());

}


/**
 * @brief 
 * 上报设备信息
 */
void box_post_cloud_device_infor(void)
{

    thing_device_infor_query();
    vTaskDelay(1000  / portTICK_PERIOD_MS);

    property_post_dev_infor(); //设备信息
    vTaskDelay(1000  / portTICK_PERIOD_MS);

    property_post_net_infor(); //网络信息
    vTaskDelay(1000  / portTICK_PERIOD_MS);

    property_post_uart_brt_infor(); //上报波特率
    vTaskDelay(1000  / portTICK_PERIOD_MS);

    thing_ota_inform_version(CFG_APP_VERSION , THING_OTA_DEFAULT);  //上报版本信息
    vTaskDelay(2000  / portTICK_PERIOD_MS);//延时等待获取设备品类  

}


/**
 * @brief 
 * 心跳上报
 * @param arg 
 */
void lyy_mqtt_tick_thread(void *pxArg)
{

    (void)pxArg;

    if(lyy_mqtt_connect_result())  {

        api_mqtt_tick();
        lyy_mqtt_monitor_heart(MQTT_WAIT_HEART);
    }

}


void lyy_mqtt_csq_thread(void *pxArg)
{
    (void)pxArg;

    if(lyy_mqtt_connect_result()) //判断链接状态
    {
        property_post_csq_level();
    }

}


void vRandomGet(char *cIp, uint32_t ulLen)
{

    if(!ulLen || ulLen < 4) {

        return ;
    }

    uint32_t ulRondom = HAL_ulGetRandom();
    
    memcpy( cIp, 
            &ulRondom, 
            sizeof(uint32_t));

}


static void lyy_mqtt_sta(unsigned char lmqSta)
{


    static uint8_t startUp = 0x00;
    static enum device_lmq_sta deviceLmqSta;


    if(lmqSta != MQTT_STATE_SUCCESS) {

        deviceLmqSta = LMQ_IDE;
        return ;
    }

    switch(deviceLmqSta)
    {

        case LMQ_IDE:
        {

            LYY_LOGD("LMQ_IDE");

            if(!startUp) {

                box_post_cloud_device_infor();
                box_get_cloud_pay_param();
                startUp = 0x01; 
            }
            deviceLmqSta = LMQ_LOGIN_OK;

        } break;

        case LMQ_LOGIN_OK   :
        {

            deviceLmqSta   = LMQ_CONNECTED; 

            APP_vLedSstSta(LED_RED, LED_STA_OFF);
            APP_vLedSstSta(LED_GREEN, LED_STA_SLOW_FLASH);

            
        } break;

        case LMQ_CONNECTED  :
        {

        } break;

        case LMQ_LOGIN_FAIL :
        {
            
        } break;

        default : break;
    }

}


/**
 * @brief 
 * mqtt 业务
 */
void lyy_mqtt_model_init(void)
{

    lyy_init_device();
    lyy_mqtt_thing_init();

    lyy_mqtt_paramInit(APP_pcGetParamDevId());
    
    lyy_mqtt_sta_proc(lyy_mqtt_sta);

    /* 软件定时，时间会有一定偏差(不可阻塞任务) / 资源不够 */
#ifdef CFG_HEAP_DEBUG

#define APP_HEAP_TASK_TIME    (2000U)

    HAL_vSoftTimeInit(&g_AppTask[eHeapMonitorTask], 
                      APP_prvHeapMonitorTask, 
                      APP_HEAP_TASK_TIME,
                      NULL);

#endif

#define LYY_MQTT_TASK_TIME    (40 * 1000U)

    HAL_vSoftTimeInit(&g_AppTask[eMqttHeartTask], 
                      lyy_mqtt_tick_thread, 
                      LYY_MQTT_TASK_TIME,
                      NULL);


#define LYY_CSQ_TASK_TIME    (5 * 60 * 1000U)

    HAL_vSoftTimeInit(&g_AppTask[eMqttCsqTask], 
                      lyy_mqtt_csq_thread, 
                      LYY_CSQ_TASK_TIME,
                      NULL);


}

