/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-10-18 16:42:53
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2025-03-06 11:36:45
 * @FilePath: \lyy_app\main\cfg\user_Config.h
 */
#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__


/* 节约任务资源接口 */
#define CFG_NO_OS     


/* 打开正式固件 */
#define CFG_RELEASE    

#ifndef CFG_RELEASE

/* 测试模式 */
//#define CFG_TEST_MODE  
                                      
/* 监控打印内存使用 */

#define CFG_HEAP_DEBUG

#endif

#ifdef CFG_TEST_MODE

#define CFG_WIFI_TEST_SSID       "ikun_5G"
#define CFG_WIFI_TEST_PWD        "ikun16888"

#endif

#define CFG_SOC_ESP8266       (1)
#define CFG_SOC_ESP8684       (2)
#define CFG_SOC_RTL8711       (3)

#define CFG_USER_SOC		(CFG_SOC_RTL8711)

//屏蔽了是生产环境，打开了是开发环境
//#define URL_TYPE_DEVELOP 

#if(CFG_USER_SOC == CFG_SOC_ESP8684)

/* 模拟串口打印 */
// #define CFG_TEST_LOG

#elif(CFG_USER_SOC == CFG_SOC_RTL8711)

/* 由于硬件定时会影响CPU相关操作所以1ms的定时器改为软件定时 */
#define CFG_RTOS_TIME  

/* 目前先用文件的形式保存下载内容 */
#define CFG_WRITE_FILE


#endif


#define CFG_PM_TJC			 (0)				
#define CFG_PM_DWIN			 (1)			

#define CFG_PM_MODEL		(CFG_PM_DWIN)


/** @brief 纸钞盒分支固件标识，业务基准 ESP32C2_ZCJ_V1.0.4，云端按 BW20 镜像升级。 */
#define CFG_APP_VERSION                         "BW20_ZCJ_V1.0.0.0"
#define CFG_SDK_VERSION                         "AMEBA_RTOS_SDK_V1.0.1"    
#define CFG_HARD_VERSION                        "BW20_LYY_V1.00"    


/* 网络配置参数 */
#define CFG_SERVER_URL                          "sgiot.starthing.com"
#define CFG_SERVER_PORT                         (8883)


/* 产品相关参数 */
#define CFG_MODEL_PRODUCT_KEY                   "Bt8e0Edyzytx"
#define CFG_MODEL_PRODUCT_SECRET                "Q3MgV9EWfgvfHRc6"
#define CFG_DEV_TYPE                            "dev_type_default"


/* wifi初始化账号密码 */
#define CFG_WIFI_SSID                           "leyaoyao_config"
#define CFG_WIFI_PWD                            "leyaoyao168"


/* 硬件初始化参数 */
#define CFG_UART_BRT                            (38400U)
#define CFG_PULSE_WIDTH                         (50U)
#define CFG_PULSE_SPACE                         (200U)

    enum {

        LYY_SUCCESS,
        LYY_FAIL,

        LYY_READ_PARAM_FAIL,
        LYY_WRITE_PARAM_FAIL

    };



#endif


