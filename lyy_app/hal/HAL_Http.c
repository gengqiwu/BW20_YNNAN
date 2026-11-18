/**
 * @file HAL_Http.c
 * @brief 单实例 HTTP/HTTPS GET：复制 URL 字段后在独立任务中解析响应并回调 body 分片。
 * @note 全局状态只支持一个活动请求，调用者不能并行下载不同资源。
 */
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


/**
 * @brief HTTP 解析器 body 回调，先同步递交分片，再累计实际接收长度。
 * @param[in] pxHttpParser 解析器上下文，当前未使用。
 * @param[in] pcAt 当前分片起始地址，仅在本次回调期间有效。
 * @param[in] xLength 分片字节数，不保证 NUL 结尾。
 * @return 固定为 0，让解析器继续接收。
 */
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

/**
 * @brief 标记 HTTP 消息已经完整解析。
 * @param[in] pxParser 解析器上下文，用于记录响应状态码。
 * @return 固定为 0。
 * @note 消息结束和业务成功不同，外层还要求状态码为 200。
 */
int lOnResponseComplete(http_parser *pxParser) 
{

    g_HalHttpC.ucHttpSta = eHttpComplete;
    
    LYY_LOGD("Response completed with status code: %d\n", pxParser->status_code);

    return 0; 

}


/**
 * @brief 在已连接 TCP/TLS 通道发送 GET，解析 body 直到完整响应或失败。
 * @param[in] ucHttpType eHttp 或 eHttps。
 * @param[in] pvArg HTTP 为 socket 句柄地址，HTTPS 为 hal_tls_t 指针。
 * @return HTTP_GET_SUCCESS 表示完整 200 响应；其他情况为 HTTP_GET_ERR。
 * @note 无数据进展 10 秒超时；分片可跨网络读取，结束长度使用实际 body 计数。
 */
static uint8_t HAL_prucHttpcOp(uint8_t ucHttpType, void *pvArg)
{


    int       lRet;
    char      *pcBuff       = NULL;
    char      *pcHttpHeard  = NULL;
    uint8_t    ucRes        = HTTP_GET_ERR;
    http_parser xHttpParser;
    http_parser_init(&xHttpParser, HTTP_RESPONSE);

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


    http_parser_settings  xHttpParserSet = {
        
        .on_body             = lBody,
        .on_message_complete = lOnResponseComplete,
    };

    
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

            size_t parsed = http_parser_execute(&xHttpParser, &xHttpParserSet, pcBuff, lRet);
            if (parsed != (size_t)lRet || HTTP_PARSER_ERRNO(&xHttpParser) != HPE_OK) break;

            ulSysTick = HAL_ulGetSysTick();

        }

        if(eHttpComplete == g_HalHttpC.ucHttpSta && xHttpParser.status_code == 200) {

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

    g_HalHttpC.ulHttpRvDataLen = g_HalHttpC.ulHttpBodyRvDataLen;

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

/**
 * @brief 独立 GET 任务，选择 HTTP/HTTPS、通知结果并恢复全局空闲状态。
 * @param[in] pvArg 任务参数，未使用。
 * @note 结果回调执行期间请求仍被标为忙，回调内不能立即复用此单实例发起新 GET。
 */
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


/**
 * @brief 校验并复制 URL 主机名/路径，再创建异步 GET 任务。
 * @param[in] pcUrl NUL 结尾的 HTTP/HTTPS URL，须包含主机后的斜杠路径。
 * @param[in] xHttpDataCd body 分片回调，可为 NULL，缓冲仅在回调期间有效。
 * @param[in] xHttpRes 结束结果回调，可为 NULL，第二参数是实际 body 字节数。
 * @note 忙、URL 无效或任务创建失败时会同步回调错误；URL 不被修改，返回后调用方可释放。
 */
void HAL_vHttpGet(const char *pcUrl, vHttpDataCd xHttpDataCd, vHttpRes xHttpRes)
{


    if(g_HalHttpC.ucHttpSta != eHttpIDle || !pcUrl) {

        LYY_LOGE(">>> http running .....");
        if (xHttpRes) xHttpRes(HTTP_GET_ERR, 0);
        return ;
    }

    g_HalHttpC.ulHttpRvDataLen      = 0x00;
    g_HalHttpC.ulHttpBodyRvDataLen  = 0x00;
    g_HalHttpC.xttpDataCd           = xHttpDataCd;
    g_HalHttpC.xHttpRes             = xHttpRes;

    const char *host = NULL;
    if (!strncmp(pcUrl, "http://", 7)) { g_HalHttpC.lPort = 80; host = pcUrl + 7; }
    else if (!strncmp(pcUrl, "https://", 8)) { g_HalHttpC.lPort = 443; host = pcUrl + 8; }
    const char *path = host ? strchr(host, '/') : NULL;
    size_t host_len = path ? (size_t)(path - host) : 0;
    if (!path || !host_len || host_len >= sizeof(g_HalHttpC.pcHostName) ||
        strlen(path) >= sizeof(g_HalHttpC.pcPath)) {
        if (xHttpRes) xHttpRes(HTTP_GET_ERR, 0);
        return;
    }
    memcpy(g_HalHttpC.pcHostName, host, host_len);
    g_HalHttpC.pcHostName[host_len] = 0;
    strcpy(g_HalHttpC.pcPath, path);

    LYY_LOGD("Hostname: %s\n",  g_HalHttpC.pcHostName);
    LYY_LOGD("Port: %d\n",      g_HalHttpC.lPort);
    LYY_LOGD("Path: %s\n",      g_HalHttpC.pcPath);

                            
#define HTTP_THREAD_NAME          "HTTP_THREAD"
#define HTTP_THREAD_STACK         (1024 * 16 / sizeof(portSTACK_TYPE))

    g_HalHttpC.ucHttpSta = eHttpCreate;
    if (xTaskCreate(HAL_prvHttpcThread, HTTP_THREAD_NAME, HTTP_THREAD_STACK, NULL, 8, NULL) != pdPASS) {
        g_HalHttpC.ucHttpSta = eHttpIDle;
        if (xHttpRes) xHttpRes(HTTP_GET_ERR, 0);
    }

}












