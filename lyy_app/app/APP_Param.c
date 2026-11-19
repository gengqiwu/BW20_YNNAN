/** @file APP_Param.c @brief 保留 BW20 文件布局，适配纸钞盒参数持久化和旧设备号兼容读取。 */
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


static APP_SER_T        g_xAppSer;
static APP_PARAM_T      g_xAppParam;
static APP_DEV_INFO_T   g_xAppDevInfo;
union U ulPulsetype;

/**
 * @brief 将 4 字节输出模式/端口配置更新到内存并保存为 ZCJ_PULSE_INFO。
 * @param[in] value bytes[0] 为软件输出模式，bytes[1] 为对端输出端口，其余保留。
 * @return LYY_SUCCESS 或 LYY_WRITE_PARAM_FAIL，文件保存失败时内存值不回滚。
 */
int APP_lPulseParamSetInfo(uint32_t value)
{
    ulPulsetype.value = value;
    return HAL_vFileWrite("ZCJ_PULSE_INFO", (uint8_t *)&ulPulsetype, sizeof(ulPulsetype)) == sizeof(ulPulsetype)
        ? LYY_SUCCESS : LYY_WRITE_PARAM_FAIL;
}

/**
 * @brief 读取独立输出配置，读不到完整 4 字节时采用脉冲/ZCJ1 默认值。
 * @return LYY_SUCCESS 表示完整读到；LYY_READ_PARAM_FAIL 表示使用内存默认值。
 * @note 默认值不会在本接口内立即写回文件。
 */
int APP_lPulseParaInit(void)
{
    if (HAL_vFileRead("ZCJ_PULSE_INFO", (uint8_t *)&ulPulsetype, sizeof(ulPulsetype)) != sizeof(ulPulsetype)) {
        ulPulsetype.value = 0;
        ulPulsetype.bytes[0] = START_TYPE_COIN;
        ulPulsetype.bytes[1] = PULSE_ZCJ_1;
        return LYY_READ_PARAM_FAIL;
    }
    return LYY_SUCCESS;
}

/** @brief 检查已经加载的服务缓存标记，不访问文件。 @return LYY_SUCCESS 或 LYY_READ_PARAM_FAIL。 */
int APP_lSerParamInit(void)
{
    return g_xAppSer.cInitMark == APP_MASK_INIT_LOGO ? LYY_SUCCESS : LYY_READ_PARAM_FAIL;
}

/**
 * @brief 适配业务参数文件读取，不改变底层短读语义。
 * @param[in] name 文件名。
 * @param[out] data 接收缓冲。
 * @param[in] len 请求字节数。
 * @return 实际读到的字节数，打开失败为 0。
 */
int32_t lyy_para_read(const char *name, uint8_t *data, uint32_t len)
{
    return HAL_vFileRead(name, data, len);
}

/**
 * @brief 适配业务参数整文件保存，原文件被截断。
 * @param[in] name 文件名。
 * @param[in] data 待保存数据。
 * @param[in] len 字节数。
 * @return 实际写入字节数，打开失败为 -1。
 */
int32_t lyy_para_write(const char *name, uint8_t *data, uint32_t len)
{
    return HAL_vFileWrite(name, data, len);
}


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


/**
 * @brief 保存 Wi-Fi 模式，供下次启动读取。
 * @param[in] xWifiMode 要保存的 Wi-Fi 模式。
 * @return APP_lParamSetInfo 的写入结果。
 */
int APP_lSetParamWifiMode(WIFI_MODE_T xWifiMode)
{
    WIFI_MODE_T previous = g_xAppParam.xWifiInfo.xMode;
    g_xAppParam.xWifiInfo.xMode = xWifiMode;
    int result = APP_lParamSetInfo(&g_xAppParam);
    if (result != LYY_SUCCESS) g_xAppParam.xWifiInfo.xMode = previous;
    return result;
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

    g_xAppSer.cInitMark = APP_MASK_INIT_LOGO;

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

    g_xAppSer.cInitMark = APP_MASK_INIT_LOGO;

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


/**
 * @brief 将设备号缓存清零后写入完整文件，允许最多 32 字节正文。
 * @param[in] pcDevId NUL 结尾设备号，NULL 或超出缓冲长度时拒绝。
 * @return LYY_SUCCESS 为完整写入，LYY_WRITE_PARAM_FAIL 为短写，LYY_FAIL 为输入非法。
 * @note 设备号字符集由工厂层校验；文件失败时内存值已经改变，调用者应读回验证。
 */
int APP_vDevInfoInit(const char *pcDevId)
{

    if(pcDevId && strlen(pcDevId) < sizeof(g_xAppDevInfo.pcDevId)) {

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



/**
 * @brief 加载设备号并兼容旧长度，再尝试初始化其他参数及读取独立输出配置。
 * @return 设备号文件大小/标记无效为 LYY_READ_PARAM_FAIL，否则为 LYY_SUCCESS。
 * @note 缺失的通用/服务参数采用默认值并尝试写回；这些写入的错误和独立输出读取失败
 *       不传播为本函数失败。空设备号由 APP 启动层继续检查。
 */
int APP_lParamInit(void)
{


    /* 获取设备号 */
    int lSize = sizeof(APP_DEV_INFO_T);
    int read_size = HAL_vFileRead(APP_DEV_INFO, (uint8_t *)&g_xAppDevInfo, lSize);
    /* Older BW20 images used 32 bytes without room for a maximum-length ID. */
    g_xAppDevInfo.pcDevId[sizeof(g_xAppDevInfo.pcDevId) - 1] = 0;
    if ((read_size != lSize && read_size != lSize - 1) ||
        g_xAppDevInfo.cInitMark != APP_MASK_INIT_LOGO) {

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


    /* 获取网络信息 */
    lSize = sizeof(APP_SER_T);

    if(lSize != HAL_vFileRead(APP_SER_INFO, (uint8_t *)&g_xAppSer, lSize) 
                                    ||  g_xAppSer.cInitMark != APP_MASK_INIT_LOGO) {

        APP_SER_T xAppSer = {   APP_MASK_INIT_LOGO,
                                CFG_MODEL_PRODUCT_KEY,
                                CFG_MODEL_PRODUCT_SECRET,
                                CFG_SERVER_PORT, 
                                CFG_SERVER_URL,
                                CFG_DEV_TYPE };

        APP_lSerSetInfo(&xAppSer);

        LYY_LOGD(">>> server init");

    }    


    APP_lPulseParaInit();
    APP_vParamDebug();
    
    return LYY_SUCCESS;
    
}






