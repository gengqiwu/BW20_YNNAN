#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "HAL_Log.h"
#include "HAL_Http.h"
#include "HAL_System.h"

#include "APP_Param.h"

#include "utils.h"
#include "cJSON.h"


#define APP_HTTP_RECV_DATA_LEN_MAX              (1024U + 512U)     

typedef enum {

    GUIDE_IDLE,
    GUIDE_RUNNING,
    GUIDE_CHECK,
    GUIDE_END,
    GUIDE_FAIL,
}APP_GUIDE_STA_T;


typedef struct {

    char             *pHttpRevData;
    uint32_t          ulDataLen;
    APP_GUIDE_STA_T   XGuideSta;
}APP_GUIDE_T;


static APP_GUIDE_T    g_xAppGuideInfo;


static void APP_prvHttpDataParse(char *pcData, uint32_t ulLen)
{
    
    if(pcData == NULL || ulLen == 0)  {

        return ;
    }  

    LYY_LOGD("%s", pcData);

    cJSON* cJsonData = NULL;
    cJSON* data = NULL;
    cJSON* productKey = NULL;
    cJSON* productSecret = NULL;
    cJSON* code = NULL;
    cJSON* message = NULL;
    cJSON* cJsonDecrypt = NULL;
    cJSON* broker = NULL;
    uint16_t deLen = 0;
    char *urlHead = NULL;
    char *urlTail = NULL;

    cJsonData = cJSON_Parse((char *)pcData);
    if(cJsonData == NULL)
    {
        LYY_LOGD("pcData cjson parse fail");
        return;
    }

    message = cJSON_GetObjectItem(cJsonData, "message");
    if((message != NULL) && (message->type == cJSON_String))
    {
        LYY_LOGD("get message=%s", message->valuestring);
    }
    else
    {
        LYY_LOGD("get message error");
    }
    

    code = cJSON_GetObjectItem(cJsonData, "code");
    if((code == NULL) || (code->type != cJSON_Number))
    {
        LYY_LOGD("get code error");
    }

    data = cJSON_GetObjectItem(cJsonData, "data");
    if((data == NULL) || (data->type != cJSON_String))
    {
        LYY_LOGD("get data error");
        cJSON_Delete(cJsonData);
        return;
    }

    LYY_LOGD("get data=%s", data->valuestring);

    uint8_t *databuff = pvPortMalloc(512);

    if(databuff == NULL)
    {
        cJSON_Delete(cJsonData);
        return;
    }    

    deLen = String2ToBytes(data->valuestring, (char *)databuff, strlen(data->valuestring));    


    deLen = utils_lAesECB128Decrypt(databuff, databuff, deLen, (uint8_t *)APP_pcGetProductSecret());

    LYY_LOGD("get Decrypt %d:", deLen);
    //LYY_LOGD_hex(databuff, deLen);

    //数据解析
    cJsonDecrypt = cJSON_Parse((char *)databuff);
    if(cJsonDecrypt == NULL)
    {
        LYY_LOGD("get cJsonDecrypt fail");
        vPortFree(databuff);
        cJSON_Delete(cJsonData);
        return;
    }

    APP_SER_T   xAppSer;

    memset((char *)&xAppSer, 0x00, sizeof(APP_SER_T));

    broker = cJSON_GetObjectItem(cJsonDecrypt, "broker");
    if((broker != NULL) && (broker->type == cJSON_String))
    {
        LYY_LOGD("get broker=%s", broker->valuestring);
        //做保存url处理
        urlHead = strstr(broker->valuestring, "mqtts://");
        if(urlHead != NULL)
        {
            urlHead+= strlen("mqtts://");
            urlTail = strstr(urlHead, ":");
            if(urlTail != NULL)
            {
                memcpy(xAppSer.pcSerUrl, urlHead, urlTail-urlHead);
                LYY_LOGD("get url=%s", xAppSer.pcSerUrl);

                urlTail += 1;//":"
                xAppSer.ulPort = atoi(urlTail);
                LYY_LOGD("get port=%d", xAppSer.ulPort);

            }            
        }
    }
    else
    {
        LYY_LOGD("get broker error");
    }

    LYY_LOGD("get cJsonDecrypt sucess");
    productKey = cJSON_GetObjectItem(cJsonDecrypt, "productKey");
    if((productKey != NULL) && (productKey->type == cJSON_String))
    {
        LYY_LOGD("get productKey:%s", productKey->valuestring);
        memset(xAppSer.pcProductKey, 0x00, sizeof(xAppSer.pcProductKey));
        memcpy(xAppSer.pcProductKey, productKey->valuestring, strlen(productKey->valuestring));
    }
    else
    {
        LYY_LOGD("get productKey error");
    }

    productSecret = cJSON_GetObjectItem(cJsonDecrypt, "productSecret");
    if((productSecret != NULL) && (productSecret->type == cJSON_String))
    {        
        LYY_LOGD("get productSecret:%s", productSecret->valuestring);
        memset(xAppSer.pcProductSecret, 0x00, sizeof(xAppSer.pcProductSecret));
        memcpy(xAppSer.pcProductSecret, productSecret->valuestring, strlen(productSecret->valuestring));
    }
    else
    {
        LYY_LOGD("get productSecret error");
    }


    vPortFree(databuff);
    cJSON_Delete(cJsonData);
    cJSON_Delete(cJsonDecrypt);


    uint8_t pucUpdate = 0x00;

    /* 保险点一个个对 */
    if(strlen((char *)xAppSer.pcProductKey) == 0) {

        strcpy(xAppSer.pcProductKey, APP_pcGetProductKey());

    }else{

        if(strcmp(xAppSer.pcProductKey, APP_pcGetProductKey())) {

            pucUpdate = 0x01;
        }
    }


    if(strlen((char *)xAppSer.pcProductSecret) == 0) {

        strcpy(xAppSer.pcProductSecret, APP_pcGetProductSecret());
    }else{

        if(strcmp(xAppSer.pcProductSecret, APP_pcGetProductSecret())) {

            pucUpdate = 0x01;

        }
    }


    if(strlen((char *)xAppSer.pcSerUrl) == 0x00) {

        strcpy(xAppSer.pcSerUrl, APP_pcGetSerUrl());

    }else{

        if(strcmp(xAppSer.pcSerUrl, APP_pcGetSerUrl())) {

            pucUpdate = 0x01;
        }
    }


    if(APP_usGetSerPort() != xAppSer.ulPort) {

        pucUpdate = 0x01;
    }
    

    if(0x01 == pucUpdate) {

        xAppSer.cInitMark = 'D';
        APP_lSerSetInfo(&xAppSer);

        HAL_vReset("guide update", 1000);

    }


    g_xAppGuideInfo.XGuideSta = GUIDE_END;

}


static void APP_prvHttpDataRvCd(char *pcData, uint32_t ulLen)
{

    if(!g_xAppGuideInfo.pHttpRevData ||  
        g_xAppGuideInfo.ulDataLen + ulLen >= APP_HTTP_RECV_DATA_LEN_MAX) {

        return ;
    }

    memcpy(g_xAppGuideInfo.pHttpRevData + g_xAppGuideInfo.ulDataLen,
           pcData,
           ulLen);

    g_xAppGuideInfo.ulDataLen += ulLen;

}



static void APP_prvHttpRes(uint8_t ucRes, uint32_t ulLen)
{

    (void)ulLen;

    if(HTTP_GET_SUCCESS == ucRes) {

    }else{

        LYY_LOGE("### HTTP GET ERR");
    }

    if( g_xAppGuideInfo.pHttpRevData && 
        g_xAppGuideInfo.ulDataLen) {

        APP_prvHttpDataParse(   g_xAppGuideInfo.pHttpRevData, 
                                g_xAppGuideInfo.ulDataLen);
    }
    

    if(g_xAppGuideInfo.pHttpRevData) {

        vPortFree(g_xAppGuideInfo.pHttpRevData);
        g_xAppGuideInfo.pHttpRevData = NULL;
    }


}


void APP_vGuideLoop(void)
{

    switch (g_xAppGuideInfo.XGuideSta)
    {

        case GUIDE_IDLE:{

            if(!g_xAppGuideInfo.pHttpRevData) {

                g_xAppGuideInfo.pHttpRevData = pvPortMalloc(APP_HTTP_RECV_DATA_LEN_MAX);

            }

            if(!g_xAppGuideInfo.pHttpRevData) {

                LYY_LOGE("### http rev data err");
                return ;
            }

            memset( g_xAppGuideInfo.pHttpRevData, 
                    0x00, 
                    APP_HTTP_RECV_DATA_LEN_MAX);

            g_xAppGuideInfo.ulDataLen = 0x00;

            uint8_t pucDviceIdStr[48]       = {0x00};
            uint8_t pucDeviceIdBuff[48]     = {0x00};
            char    pcGuideUrl[256]         = {0x00};
            
            int lLen = utils_lAesECB128Encrypt((uint8_t *)APP_pcGetParamDevId(), pucDeviceIdBuff, 
                                        strlen(APP_pcGetParamDevId()), (uint8_t *)APP_pcGetProductSecret());

            utils_lHexToStringLower((char *)pucDviceIdStr, (uint8_t *)pucDeviceIdBuff, lLen);

            // sprintf(pcGuideUrl,     "https://sgiot.starthing.com/openapi/v1/dev/%s/device?deviceId=%s",
            //                         APP_pcGetProductKey(),
            //                         pucDviceIdStr);

#ifdef URL_TYPE_DEVELOP
            LYY_LOGD("develop guide url");
            // 开发环境
            sprintf(pcGuideUrl, "https://db.cadmem.com/openapi/v1/dev/%s/device?deviceId=%s",
                    APP_pcGetProductKey(),
                    pucDviceIdStr);

#else
            LYY_LOGD("release guide url");
            // 正式环境
            sprintf(pcGuideUrl, "https://sgiot.starthing.com/openapi/v1/dev/%s/device?deviceId=%s",
                    APP_pcGetProductKey(),
                    pucDviceIdStr);
#endif // DEBUG

            LYY_LOGD(">>> URL : [%s]", pcGuideUrl);

            g_xAppGuideInfo.XGuideSta = GUIDE_RUNNING;

            HAL_vHttpGet((const char *)pcGuideUrl, 
                        APP_prvHttpDataRvCd, 
                        APP_prvHttpRes);

        }break;

        case GUIDE_CHECK :{

        }break;
        
        default:
            break;

    }

}


