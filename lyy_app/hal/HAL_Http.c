#include <stdint.h>
#include <string.h>

#include "HAL_Tcp.h"
#include "HAL_Tls.h"
#include "HAL_Log.h"
#include "HAL_Http.h"
#include "HAL_System.h"

#include "http_parser.h"


enum {

    eHttpIDle,
    eHttpCreate,
    eHttpRecving,
    eHttpErr,
    eHttpComplete,
};


enum {

    eHttp,
    eHttps,
};


typedef struct {

    vHttpDataCd         xttpDataCd;
    vHttpRes            xHttpRes;
    int                 lPort;
    char                pcHostName[256];
    char                pcPath[512];
    uint8_t             ucHttpSta;
    uint32_t            ulHttpRvDataLen;
    uint32_t            ulHttpBodyRvDataLen;

}HAL_HTTPC_T;


static HAL_HTTPC_T g_HalHttpC;


int lBody(http_parser *pxHttpParser, const char *pcAt, size_t xLength)
{

	(void)pxHttpParser;
    (void)pcAt;

    g_HalHttpC.ucHttpSta = eHttpRecving;

    if(g_HalHttpC.xttpDataCd) {

        g_HalHttpC.xttpDataCd((char *)pcAt, xLength);
    }

    g_HalHttpC.ulHttpBodyRvDataLen += xLength;

    LYY_LOGD(">> pcAt len: [%ld]", xLength);

	return 0;

}

int lOnResponseComplete(http_parser *pxParser) 
{

    g_HalHttpC.ucHttpSta = eHttpComplete;
    
    LYY_LOGD("Response completed with status code: %d\n", pxParser->status_code);

    return 0; 

}


static uint8_t HAL_prucHttpcOp(uint8_t ucHttpType, void *pvArg)
{


    int       lRet;
    char      *pcBuff       = NULL;
    char      *pcHttpHeard  = NULL;
    uint8_t    ucRes        = HTTP_GET_ERR;

#define  HAL_HEARD_LEN_MAX      (1024U)
#define  HAL_BUFF_LEN_MAX       (10 * 1024U)

    pcBuff = pvPortMalloc(HAL_BUFF_LEN_MAX);

    if(!pcBuff) {

        goto httpc_exit;
    }

    pcHttpHeard = pvPortMalloc(HAL_HEARD_LEN_MAX);

    if(!pcHttpHeard) {

        goto httpc_exit;
    }

    memset( pcHttpHeard, 
            0x00, 
            HAL_HEARD_LEN_MAX);

    sprintf(pcHttpHeard, "GET %s HTTP/1.1\r\nHost: %s\r\n\r\n", g_HalHttpC.pcPath,
                                                                g_HalHttpC.pcHostName);

    LYY_LOGD(">>>> pcHttpHeard : %s", pcHttpHeard);


    if(eHttp == ucHttpType) {

        lRet =  HAL_ulTcpWrite( *(uint32_t *)pvArg,  
                                pcHttpHeard, 
                                strlen(pcHttpHeard), 
                                1000);
    }else {

        lRet =  HAL_lTlsWrite(  (hal_tls_t *)pvArg,  
                                pcHttpHeard, 
                                strlen(pcHttpHeard), 
                                1000);
    }

    if(lRet <= 0x00) {
        
        goto httpc_exit;
    }

    http_parser           xHttpParser;

    http_parser_settings  xHttpParserSet = {
        
        .on_body             = lBody,
        .on_message_complete = lOnResponseComplete,
    };

    http_parser_init(&xHttpParser, HTTP_RESPONSE);
    
    g_HalHttpC.ucHttpSta = eHttpRecving;  

#define HAL_HTTP_GET_TIMEOUT            (1000 * 10U)

    uint32_t ulSysTick = HAL_ulGetSysTick();

    while(1) {

        vTaskDelay(1  / portTICK_PERIOD_MS);

        memset(pcBuff, 0x00, HAL_BUFF_LEN_MAX);

        if(eHttp == ucHttpType) {

            lRet = HAL_lTcpRead(    *(uint32_t *)pvArg, 
                                    pcBuff, 
                                    HAL_BUFF_LEN_MAX - 1, 
                                    100);
        }else{

            lRet = HAL_lTlsRead((hal_tls_t *)pvArg, 
                                pcBuff, 
                                HAL_BUFF_LEN_MAX - 1, 
                                0);
        }



        if(lRet > 0)  {

            http_parser_execute(&xHttpParser, 
                                &xHttpParserSet, 
                                pcBuff, 
                                lRet); 

            if( xHttpParser.status_code != 200 &&
                xHttpParser.status_code != 206) {
                
                LYY_LOGE("### xHttpParser.status_code : [%d]", xHttpParser.status_code);
                break;
            }

            ulSysTick = HAL_ulGetSysTick();

        }

        if(eHttpComplete == g_HalHttpC.ucHttpSta) {

            ucRes = HTTP_GET_SUCCESS;
            break;
        }

        if(lRet < 0) {

            break;
        }

        if(HAL_ucTickTimeout(ulSysTick, HAL_HTTP_GET_TIMEOUT)) {

            LYY_LOGE("### handshake timeout : [%d]", HAL_HTTP_GET_TIMEOUT);
            break;
        }

    }

httpc_exit:

    g_HalHttpC.ulHttpRvDataLen = xHttpParser.content_length ? 
                        xHttpParser.content_length : g_HalHttpC.ulHttpBodyRvDataLen;

    if(pcBuff) {

        vPortFree(pcBuff);
    }

    if(pcHttpHeard) {

        vPortFree(pcHttpHeard);
    } 

    return ucRes;

}


static uint8_t HAL_prucHttpcsGet(void)
{


    hal_tls_t *pxTls   = NULL;
    uint8_t    ucRes   = HTTP_GET_ERR;

    pxTls = HAL_vTlsCreat();

    if(!pxTls) {

        goto httpcs_exit;
    }

    int lRet = HAL_lTlsEstablish(   pxTls, 
                                    g_HalHttpC.pcHostName , 
                                    strlen(g_HalHttpC.pcHostName), 
                                    g_HalHttpC.lPort,  
                                    1000 * 10);

    if(lRet == 0x01) {

        LYY_LOGD(">>>> HTTP TLS CONNECT SUCCESS");

    }else{

        goto httpcs_exit;
    } 
    
    ucRes = HAL_prucHttpcOp(eHttps, (void *)pxTls);

httpcs_exit : 

    if(pxTls) {

        HAL_vTlsClose(pxTls);
    }

    return ucRes;

}


static uint8_t HAL_prucHttpcGet(void)
{

    uint8_t  ucRes = HTTP_GET_ERR;

    uint32_t ulFd  = HAL_ulTcpEstablish( (const char *)g_HalHttpC.pcHostName , 
                                        g_HalHttpC.lPort);

    if(ulFd <= 0x00)  {

        LYY_LOGE("###  tcp connect err");
        return HTTP_GET_ERR;
    }

    ucRes = HAL_prucHttpcOp(eHttp, (void *)&ulFd);

    HAL_ulTcpDestroy(ulFd);

    return ucRes;

}

static void HAL_prvHttpcThread(void *pvArg)
{

    (void)pvArg;

    uint8_t ucRes = HTTP_GET_ERR;

    if(443 == g_HalHttpC.lPort) {
        
        ucRes = HAL_prucHttpcsGet();

    }else{

        ucRes = HAL_prucHttpcGet();
    }

    if(g_HalHttpC.xHttpRes) {

        g_HalHttpC.xHttpRes(ucRes, g_HalHttpC.ulHttpRvDataLen);
    }

    g_HalHttpC.ucHttpSta = eHttpIDle;

    vTaskDelete(NULL);

}


void HAL_vHttpGet(const char *pcUrl, vHttpDataCd xHttpDataCd, vHttpRes xHttpRes)
{


    if(g_HalHttpC.ucHttpSta != eHttpIDle || !pcUrl) {

        LYY_LOGE(">>> http running .....");
        return ;
    }

    g_HalHttpC.ulHttpRvDataLen      = 0x00;
    g_HalHttpC.ulHttpBodyRvDataLen  = 0x00;
    g_HalHttpC.xttpDataCd           = xHttpDataCd;
    g_HalHttpC.xHttpRes             = xHttpRes;

    char *pcPath;
    char *pcHostname;
    

    if (strncmp(pcUrl, "http://", 7) == 0) {

        g_HalHttpC.lPort = 80;
        pcHostname = (char *)pcUrl + 7;

    } else if (strncmp(pcUrl, "https://", 8) == 0) {

        g_HalHttpC.lPort = 443;
        pcHostname = (char *)pcUrl + 8;

    } else {

        if(g_HalHttpC.xHttpRes) {

            g_HalHttpC.xHttpRes(HTTP_GET_ERR, g_HalHttpC.ulHttpRvDataLen);
        }
        LYY_LOGE("Invalid URL format\n");
        return ;
    }

    pcPath = strchr(pcHostname, '/');

    if (pcPath == NULL) {

        if(g_HalHttpC.xHttpRes) {

            g_HalHttpC.xHttpRes(HTTP_GET_ERR, g_HalHttpC.ulHttpRvDataLen);
        }
        LYY_LOGE("Invalid URL format\n");
        return ;
    }

    *pcPath = '\0';

    memset( g_HalHttpC.pcHostName, 
            0x00, 
            sizeof(g_HalHttpC.pcHostName));

    if(strlen(pcHostname) > sizeof(g_HalHttpC.pcHostName)) {

        if(g_HalHttpC.xHttpRes) {

            g_HalHttpC.xHttpRes(HTTP_GET_ERR, g_HalHttpC.ulHttpRvDataLen);
        }
        LYY_LOGE("### pcHostname len err ");
        return ;
    }

    strcpy(g_HalHttpC.pcHostName, pcHostname);

    *pcPath = '/';

    if(strlen(pcPath) > sizeof(g_HalHttpC.pcPath)) {

        if(g_HalHttpC.xHttpRes) {

            g_HalHttpC.xHttpRes(HTTP_GET_ERR, g_HalHttpC.ulHttpRvDataLen);
        }
        LYY_LOGE("### pcPath len err ");
        return ;
    }

    memset( g_HalHttpC.pcPath, 
            0x00, 
            sizeof(g_HalHttpC.pcPath));

    strcpy(g_HalHttpC.pcPath, pcPath);

    LYY_LOGD("Hostname: %s\n",  g_HalHttpC.pcHostName);
    LYY_LOGD("Port: %d\n",      g_HalHttpC.lPort);
    LYY_LOGD("Path: %s\n",      g_HalHttpC.pcPath);

                            
#define HTTP_THREAD_NAME          "HTTP_THREAD"
#define HTTP_THREAD_STACK         (1024 * 16 / sizeof(portSTACK_TYPE))

    xTaskCreate(    HAL_prvHttpcThread, 
                    HTTP_THREAD_NAME, 
                    HTTP_THREAD_STACK, 
                    NULL, 
                    8,
                    NULL);

    g_HalHttpC.ucHttpSta = eHttpCreate;

}












