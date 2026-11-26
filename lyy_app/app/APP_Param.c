/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-10-13 18:30:02
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2025-03-06 13:44:25
 * @FilePath: \lyy_app\main\app\APP_Param.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "USER_Config.h"

#include "HAL_Log.h"
#include "HAL_Flash.h"

#include "APP_Param.h"

#include "lyy_start.h"

#define APP_DEV_INFO               "APP_DEV_INFO"           
#define APP_SER_INFO               "APP_SER_INFO"
#define APP_PARA_INFO              "APP_PARA_INFO"

#define APP_MASK_INIT_LOGO         'D'

/** @brief APP_SER_INFO 缓存所属环境；不改变设备参数的旧初始化标记。 */
#ifdef URL_TYPE_DEVELOP
#define APP_SER_ENV_MARK           'T'
#else
#define APP_SER_ENV_MARK           'P'
#endif


static APP_SER_T        g_xAppSer;
static APP_PARAM_T      g_xAppParam;
static APP_DEV_INFO_T   g_xAppDevInfo;


char *APP_pcGetSerUrl(void)
{
    return g_xAppSer.pcSerUrl;
}

uint16_t APP_usGetSerPort(void)
{
    return g_xAppSer.ulPort;
}


char *APP_pcGetProductKey(void)
{
    return g_xAppSer.pcProductKey;
}

char *APP_pcGetProductSecret(void)
{
    return g_xAppSer.pcProductSecret;
}


char *APP_pcGetDevType(void)
{
    return g_xAppSer.pcDevType;
}


char *APP_pcGetParamWifiSsid(void)
{
    return g_xAppParam.xWifiInfo.pcSsid;
}


char *APP_pcGetParamWifiPwd(void)
{
    return g_xAppParam.xWifiInfo.pcPwd;
}


void APP_vSetParamWifiMode(WIFI_MODE_T xWifiMode)
{
    g_xAppParam.xWifiInfo.xMode = xWifiMode;
    APP_lParamSetInfo(&g_xAppParam);
}


WIFI_MODE_T APP_xGetParamWifiMode(void)
{
    return g_xAppParam.xWifiInfo.xMode;
}


char *APP_pcGetParamDevId(void)
{
    return g_xAppDevInfo.pcDevId;
}


uint32_t APP_ulGetParamBaudRate(void)
{
    return g_xAppParam.ulBaudRate;
}


APP_ACC_T *APP_pxlGetParameAcc(void)
{
    return &g_xAppParam.xDevAcc;
}


APP_PULSE_T *APP_pxlGetParamePulse(void)
{
    return &g_xAppParam.xDevPulse;
}


int APP_lSetParamUpdate(void)
{
    return APP_lParamSetInfo(&g_xAppParam);
}



int APP_lParamSetInfo(APP_PARAM_T *pxAppParam)
{

    memcpy((char *)&g_xAppParam, (const char *)pxAppParam, sizeof(APP_PARAM_T));

    int lSize = sizeof(APP_PARAM_T);

    g_xAppParam.cInitMark = APP_MASK_INIT_LOGO;

    if(lSize != HAL_vFileWrite(APP_PARA_INFO, (uint8_t *)&g_xAppParam, lSize)) {

        LYY_LOGE("### Write ERR");

        return LYY_WRITE_PARAM_FAIL;
    }

    return LYY_SUCCESS;
}



int APP_lserSetDevTypeInfo(char *pcDevType)
{

    if(!pcDevType) {

        return LYY_FAIL;
    }

    memset(g_xAppSer.pcDevType, 0x00, sizeof(g_xAppSer.pcDevType));
    strcpy(g_xAppSer.pcDevType, pcDevType);

    int lSize = sizeof(APP_SER_T);

    g_xAppSer.cInitMark = APP_SER_ENV_MARK;

    if(lSize != HAL_vFileWrite(APP_SER_INFO, (uint8_t *)&g_xAppSer, lSize)) {

        LYY_LOGE("### Write ERR");

        return LYY_WRITE_PARAM_FAIL;
    }

    return LYY_SUCCESS;

}


int APP_lParamSetParamePulse(APP_PULSE_T *pxApluse)
{

    memcpy( (char *)&g_xAppParam.xDevPulse, 
            (char *)pxApluse, 
            sizeof(APP_PULSE_T));

    return APP_lParamSetInfo(&g_xAppParam);

}


int APP_lSerSetInfo(APP_SER_T *pxAppSer)
{

    memcpy((char *)&g_xAppSer, (const char *)pxAppSer, sizeof(APP_SER_T));

    int lSize = sizeof(APP_SER_T);

    g_xAppSer.cInitMark = APP_SER_ENV_MARK;

    if(lSize != HAL_vFileWrite(APP_SER_INFO, (uint8_t *)&g_xAppSer, lSize)) {

        LYY_LOGE("### Write ERR");

        return LYY_WRITE_PARAM_FAIL;
    }

    return LYY_SUCCESS;

}


int APP_lParamSetBaundRate(uint32_t ulBaundRate)
{
    g_xAppParam.ulBaudRate = ulBaundRate;

    return APP_lParamSetInfo(&g_xAppParam);
}


int APP_lParamSetWifiInfo(WIFI_INFO_T *pcWifiInfo)
{

    memcpy( (char *)&g_xAppParam.xWifiInfo, 
            (const char *)pcWifiInfo, 
            sizeof(WIFI_INFO_T));

    
    return APP_lParamSetInfo(&g_xAppParam);
}


int APP_vDevInfoInit(const char *pcDevId)
{

    if(pcDevId) {

        memset( g_xAppDevInfo.pcDevId, 
                0x00, 
                sizeof(g_xAppDevInfo.pcDevId));

        strcpy( g_xAppDevInfo.pcDevId, 
                pcDevId);

        g_xAppDevInfo.cInitMark = APP_MASK_INIT_LOGO;

        int lSize = sizeof(APP_DEV_INFO_T);

        if(lSize != HAL_vFileWrite( APP_DEV_INFO, 
                                    (uint8_t *)&g_xAppDevInfo, 
                                    lSize)) {

            LYY_LOGE("### Write ERR");

            return LYY_WRITE_PARAM_FAIL;
        }

        return LYY_SUCCESS;

    }  

    return LYY_FAIL;

}


static void APP_vParamDebug(void)
{
    LYY_LOGD("");
    LYY_LOGD("========================================================================= ");
    LYY_LOGD("|     param                     |  info                                 | ");
    LYY_LOGD("========================================================================= ");  
    LYY_LOGD("|                           wifi  info                                  | ");
    LYY_LOGD("========================================================================= ");
    LYY_LOGD("|WIFI SSID                      |     %-34s|", g_xAppParam.xWifiInfo.pcSsid);
    LYY_LOGD("|WIFI PWD                       |     %-34s|", g_xAppParam.xWifiInfo.pcPwd);    
    LYY_LOGD("|WIFI MODE                      |     %-34s|", g_xAppParam.xWifiInfo.xMode == WIFI_STA ? "STA" : "AP");
    LYY_LOGD("========================================================================= ");  
    LYY_LOGD("|                           VER  info                                   | ");
    LYY_LOGD("========================================================================= ");
    LYY_LOGD("|APP VER                        |     %-34s|", CFG_APP_VERSION); 
    LYY_LOGD("|SDK VER                        |     %-34s|", CFG_SDK_VERSION);
    LYY_LOGD("========================================================================= ");    
    LYY_LOGD("========================================================================= ");  
    LYY_LOGD("|                           acc  info                                   | ");
    LYY_LOGD("========================================================================= ");
    LYY_LOGD("|GIFT OUT                       |     %-34d|", g_xAppParam.xDevAcc.ulGiftOutSum); 
    LYY_LOGD("|COIN IN                        |     %-34d|", g_xAppParam.xDevAcc.ulCoinInSum);
    LYY_LOGD("|COIN OUT                       |     %-34d|", g_xAppParam.xDevAcc.ulCoinOutSum);
    LYY_LOGD("========================================================================= ");  
    LYY_LOGD("========================================================================= ");  
    LYY_LOGD("|                           pulse  info                                  | ");
    LYY_LOGD("========================================================================= ");   
    LYY_LOGD("|WIDTH                          |     %-34d|", g_xAppParam.xDevPulse.ulPulseWidth); 
    LYY_LOGD("|SPACE                          |     %-34d|", g_xAppParam.xDevPulse.ulPulseSpace);  
    LYY_LOGD("========================================================================= ");  
    LYY_LOGD("========================================================================= ");
    LYY_LOGD("|     server                     |  info                                | ");
    LYY_LOGD("========================================================================= ");  
    LYY_LOGD("|                           server  info                                | ");
    LYY_LOGD("========================================================================= ");
    LYY_LOGD("|server url                     |     %-34s|", g_xAppSer.pcSerUrl);
    LYY_LOGD("|server port                    |     %-34d|", g_xAppSer.ulPort);    
    LYY_LOGD("========================================================================= ");  
    LYY_LOGD("|                           Product  info                               | ");
    LYY_LOGD("========================================================================= ");
    LYY_LOGD("|ProductKey                     |     %-34s|", g_xAppSer.pcProductKey); 
    LYY_LOGD("|ProductSecre                   |     %-34s|", g_xAppSer.pcProductSecret);
    LYY_LOGD("========================================================================= ");  
}



int APP_lParamInit(void)
{


    /* 获取设备号 */
    int lSize = sizeof(APP_DEV_INFO_T);

    if(lSize != HAL_vFileRead(APP_DEV_INFO, (uint8_t *)&g_xAppDevInfo, lSize) 
                                    ||  g_xAppDevInfo.cInitMark != APP_MASK_INIT_LOGO) {


        LYY_LOGE("### READ Devid ERR");

        g_xAppParam.xDevPulse.ulPulsetype   = START_TYPE_COIN;
        g_xAppParam.xDevPulse.ulPulseSpace  = CFG_PULSE_SPACE;
        g_xAppParam.xDevPulse.ulPulseWidth  = CFG_PULSE_WIDTH;

        return LYY_READ_PARAM_FAIL;
    }


    lSize = sizeof(APP_PARAM_T);

    /* 获取设备信息 */
    if(lSize != HAL_vFileRead(APP_PARA_INFO, (uint8_t *)&g_xAppParam, lSize) 
                                    ||  g_xAppParam.cInitMark != APP_MASK_INIT_LOGO) {


        APP_PARAM_T xAppParam = {   APP_MASK_INIT_LOGO,
                                    CFG_UART_BRT,
                                    {CFG_WIFI_SSID,     CFG_WIFI_PWD,       WIFI_STA}, 
                                    {0x00,              0x00,               0x00},         
                                    {START_TYPE_COIN,   CFG_PULSE_WIDTH,   CFG_PULSE_SPACE}};

        APP_lParamSetInfo(&xAppParam);

        LYY_LOGD(">>> param init");

    }    


    /* 服务器缓存按环境隔离。旧版 'D' 缓存没有环境信息，迁移一次。 */
    lSize = sizeof(APP_SER_T);
    memset(&g_xAppSer, 0, sizeof(g_xAppSer));
    int serReadLen = HAL_vFileRead(APP_SER_INFO, (uint8_t *)&g_xAppSer, lSize);
    if (serReadLen != lSize || g_xAppSer.cInitMark != APP_SER_ENV_MARK) {
        LYY_LOGI("server cache reset: read_len=%d, mark=0x%02x, expected=%c",
                 serReadLen, (unsigned int)(unsigned char)g_xAppSer.cInitMark, APP_SER_ENV_MARK);
        APP_SER_T xAppSer = { APP_SER_ENV_MARK,
                              CFG_MODEL_PRODUCT_KEY,
                              CFG_MODEL_PRODUCT_SECRET,
                              CFG_SERVER_PORT,
                              CFG_SERVER_URL,
                              CFG_DEV_TYPE };

        /* 设备类型不随环境切换。只从完整且可识别的旧记录保留它。 */
        if (serReadLen == lSize && (g_xAppSer.cInitMark == APP_MASK_INIT_LOGO ||
            g_xAppSer.cInitMark == 'P' || g_xAppSer.cInitMark == 'T') &&
            g_xAppSer.pcDevType[0] != '\0' &&
            memchr(g_xAppSer.pcDevType, '\0', sizeof(g_xAppSer.pcDevType)) != NULL) {
            memcpy(xAppSer.pcDevType, g_xAppSer.pcDevType, sizeof(xAppSer.pcDevType));
        }
        APP_lSerSetInfo(&xAppSer);
        LYY_LOGD(">>> server init");
    }


    APP_vParamDebug();
    
    return LYY_SUCCESS;
    
}






