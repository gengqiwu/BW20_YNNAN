#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ameba.h"

#include "HAL_Tls.h"
#include "HAL_Log.h"
#include "HAL_System.h"

#include "lwip/netdb.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/def.h"
#include <lwip/sockets.h>
#include <lwip/inet.h>

#if(CFG_USER_SOC == CFG_SOC_RTL8711)

static int HAL_prlRtl8711Random(void *p_rng, unsigned char *output, size_t output_len)
{
	(void)p_rng;
	TRNG_get_random_bytes(output, output_len);
	return 0;
}

#endif


int HAL_lTlsRead(hal_tls_t *pxTls, char *pcBuf, int lLen, uint32_t ulTimeOutMs)
{

    int  lRet;

    if (NULL == pxTls) {

        LYY_LOGE( "### tls read pxTls NULL");
        return -1;
    }

    fd_set xReadSet;
    fd_set xErrSet;

    FD_ZERO(&xReadSet);
    FD_ZERO(&xErrSet);

    FD_SET(pxTls->xServerFd.fd, &xReadSet);
    FD_SET(pxTls->xServerFd.fd, &xErrSet);

    struct timeval xTimeout = {ulTimeOutMs / 1000, (ulTimeOutMs % 1000) * 1000};

    lRet = select(  pxTls->xServerFd.fd + 1, 
                    &xReadSet, 
                    NULL, 
                    &xErrSet, 
                    &xTimeout);

    if(lRet <= 0)
    {
        return lRet;
    }

    if (lRet > 0 && FD_ISSET(pxTls->xServerFd.fd, &xErrSet)) {

        int         lSockErrno      = 0;
        socklen_t    ulOptlen       = sizeof(lSockErrno);

        getsockopt( pxTls->xServerFd.fd, 
                    SOL_SOCKET, 
                    SO_ERROR,
                    &lSockErrno,
                    &ulOptlen);

        LYY_LOGE( "### ssl_poll_read select error %d, errno = %s, fd = %d", lSockErrno, 
                                                                            strerror(lSockErrno), 
                                                                            pxTls->xServerFd.fd);

        return -1;
    }

    lRet = mbedtls_ssl_read(&pxTls->xSsl, 
                            (void *)pcBuf, 
                            lLen - 1);

    if (lRet < 0)  {
        
        LYY_LOGE( "### mbedtls_ssl_read error, errno:%s", strerror(errno));
    }

    if(lRet == 0) {

        lRet = -1;
    }

    return lRet;

}



int HAL_lTlsWrite(hal_tls_t *pxTls, char *pcBuf, int lLen, uint32_t ulTimeOutMs)
{

    int lRet;

    if(NULL == pxTls)  {

        LYY_LOGE("### tls write pxTls NULL");
        return -1;
    }

    fd_set xWrite;
    fd_set xErrset;

    FD_ZERO(&xWrite);
    FD_ZERO(&xErrset);

    FD_SET(pxTls->xServerFd.fd, &xWrite);
    FD_SET(pxTls->xServerFd.fd, &xErrset);


    struct timeval xTimeout = {ulTimeOutMs / 1000, (ulTimeOutMs % 1000) * 1000};

    lRet = select(  pxTls->xServerFd.fd + 1, 
                    NULL, 
                    &xWrite, 
                    &xErrset, 
                    &xTimeout);
    
    if(lRet <= 0) {

        LYY_LOGE("### write select lRet : [%d]  pxTls->sockfd : [%d]", lRet, pxTls->xServerFd.fd);
        return lRet;
    }

    if(lRet > 0 && FD_ISSET(pxTls->xServerFd.fd, &xErrset)) {

        int         lSockErrno  = 0;
        socklen_t   ulOptlen    = sizeof(lSockErrno);

        getsockopt( pxTls->xServerFd.fd, 
                    SOL_SOCKET, 
                    SO_ERROR, 
                    &lSockErrno, 
                    &ulOptlen);

        LYY_LOGE("### ssl_poll_write select error %d, errno = %s, fd = %d", lSockErrno, 
                                                                            strerror(lSockErrno), 
                                                                            pxTls->xServerFd.fd);

        return -1;
    }

    lRet = mbedtls_ssl_write(   &pxTls->xSsl, 
                                (const void *)pcBuf, 
                                lLen);

    if(lRet < 0) {

        LYY_LOGE("### mbedtls_ssl_write error, errno [%s]", strerror(errno));
    }

    return lRet;

}



int HAL_lTlsEstablish(hal_tls_t *pxTls, const char *pcHostname, uint16_t usHostlen, uint16_t usPort, uint32_t ulTimeOutMs)
{
    
    (void)usHostlen;

    int     lRetRe;
    int     lRet            = 0x00;
    int     lSockopt        = 0x01;

    char    pcProtStr[6]    = {0x00, };

    sprintf((char *)pcProtStr, "%u", usPort);

#if(CFG_USER_SOC != CFG_SOC_RTL8711)
    /* 生成随机数 */
    if ((lRetRe = mbedtls_ctr_drbg_seed(&pxTls->xCtrDrbg, 
                                        mbedtls_entropy_func, 
                                        &pxTls->xEntropy, 
                                        NULL, 
                                        0)) != 0) {

		LYY_LOGE("### ERROR: mbedtls_ctr_drbg_seed ret(%d)\n", lRetRe);

		goto estab_exit;
    }
#endif

    /* 连接服务 */
	if((lRetRe = mbedtls_net_connect(   &pxTls->xServerFd, 
                                        pcHostname, 
                                        pcProtStr, 
                                        MBEDTLS_NET_PROTO_TCP)) != 0) {

		LYY_LOGE("### ERROR: mbedtls_net_connect ret(%d)\n", lRetRe);

		goto estab_exit;
	}

    setsockopt( pxTls->xServerFd.fd, 
                SOL_SOCKET,  
                SO_REUSEADDR, 
                &lSockopt, 
                sizeof(lSockopt));

    setsockopt( pxTls->xServerFd.fd, 
                SOL_SOCKET,  
                SO_REUSEPORT, 
                &lSockopt, 
                sizeof(lSockopt));

    /* (tcp 8K) */
    int lBuffSendSize = 6 *  1024;           
    int lBuffRecvSize = 10 * 1024;                                         

    setsockopt( pxTls->xServerFd.fd, 
                SOL_SOCKET, 
                SO_SNDBUF,  
                &lBuffSendSize, 
                sizeof(int));

    setsockopt( pxTls->xServerFd.fd, 
                SOL_SOCKET, 
                SO_RCVBUF,  
                &lBuffRecvSize, 
                sizeof(int));

#if 0
    setsockopt( pxTls->xServerFd.fd, 
                IPPROTO_TCP, 
                TCP_NODELAY,  
                &lSockopt, 
                sizeof(lSockopt));
#endif

    /* 设置 SSL/TLS 配置 */
	if ((lRetRe = mbedtls_ssl_config_defaults(  &pxTls->xConf,
                                                MBEDTLS_SSL_IS_CLIENT,
                                                MBEDTLS_SSL_TRANSPORT_STREAM,
                                                MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {

		LYY_LOGE("### ERROR: mbedtls_ssl_config_defaults ret(%d)\n", lRetRe);

		goto estab_exit;
	}

    /* 设置随机数生成器 */
#if(CFG_USER_SOC == CFG_SOC_RTL8711)

    mbedtls_ssl_conf_rng(   &pxTls->xConf,
                            HAL_prlRtl8711Random, 
                            NULL);
#else
    
    mbedtls_ssl_conf_rng(   &pxTls->xConf,
                            mbedtls_ctr_drbg_random, 
                            &pxTls->xCtrDrbg);
#endif

    /* 不验证证书 */
    mbedtls_ssl_conf_authmode(  &pxTls->xConf, 
                                MBEDTLS_SSL_VERIFY_NONE);

    /* 按配置进行 SSL/TLS 初始化 */
    if ((lRetRe = mbedtls_ssl_setup(&pxTls->xSsl, 
                                    &pxTls->xConf)) != 0) {
        
		LYY_LOGE("### ERROR: mbedtls_ssl_setup ret(%d)\n", lRetRe);

		goto estab_exit;
    }

    /* 将套接字与 SSL/TLS 关联 */
    mbedtls_ssl_set_bio(&pxTls->xSsl, 
                        &pxTls->xServerFd, 
                        mbedtls_net_send, 
                        mbedtls_net_recv, 
                        NULL);

    uint32_t ulSysTick = HAL_ulGetSysTick();

    for( ; ; ) {

        if((lRetRe = mbedtls_ssl_handshake(&pxTls->xSsl)) != 0) {

            if (lRetRe != MBEDTLS_ERR_SSL_WANT_READ && 
                lRetRe != MBEDTLS_ERR_SSL_WANT_WRITE) {

                LYY_LOGE("### ERROR: mbedtls_ssl_handshake ret(-0x%x)", -lRetRe);
                goto estab_exit;   
            }

        }else {

            break;
        }

        if(HAL_ucTickTimeout(ulSysTick, ulTimeOutMs)) {

            LYY_LOGE("### handshake timeout : [%d]", ulTimeOutMs);
            goto estab_exit;
        }

    }

    LYY_LOGI("SSL CONENCT ret : [%d] : fd : [%d]", lRet, pxTls->xServerFd.fd);

    lRet = 0x01;

estab_exit:

    return lRet;

}



hal_tls_t *HAL_vTlsCreat(void)
{

    hal_tls_t *pxTls = NULL;

    pxTls = pvPortMalloc(sizeof(hal_tls_t));
    
    if(!pxTls) {

        LYY_LOGE("### pxTls NULL");
        return NULL;
    }

    /* 初始化网络上下文结构体 */
    mbedtls_net_init(&pxTls->xServerFd);

    /* 初始化 SSL 上下文结构体 */
    mbedtls_ssl_init(&pxTls->xSsl);

    /* 初始化 SSL 配置结构体 */
    mbedtls_ssl_config_init(&pxTls->xConf);

#if(CFG_USER_SOC != CFG_SOC_RTL8711)

    /* 初始化熵上下文 */
    mbedtls_entropy_init(&pxTls->xEntropy);

    /* 初始化 CTR-DRBG（确定性随机比特生成器）上下文 */
    mbedtls_ctr_drbg_init(&pxTls->xCtrDrbg);

#endif

    return pxTls;

}


void HAL_vTlsClose(hal_tls_t *pxTls)
{

    if(pxTls) {

        /* 发送关闭通知以安全地断开连接 */
        mbedtls_ssl_close_notify(&pxTls->xSsl);

        /* 释放资源 */
        mbedtls_net_free(&pxTls->xServerFd);
        mbedtls_ssl_free(&pxTls->xSsl);
        mbedtls_ssl_config_free(&pxTls->xConf);

#if(CFG_USER_SOC != CFG_SOC_RTL8711)

        mbedtls_entropy_free(&pxTls->xEntropy);
        mbedtls_ctr_drbg_free(&pxTls->xCtrDrbg);
#endif

        LYY_LOGD("tls close");

        vPortFree(pxTls);
        pxTls = NULL;

    }

}

