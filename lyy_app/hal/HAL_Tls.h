/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-11-27 14:10:11
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-11-27 17:58:04
 * @FilePath: \ameba-rtos\lyy_app\hal\HAL_Tls.h
 */
#ifndef __HAL_TLS_H__
#define __HAL_TLS_H__


#include "mbedtls/net_sockets.h"
#include "mbedtls/error.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "USER_Config.h"

typedef struct {

    mbedtls_net_context         xServerFd;
    mbedtls_ssl_context         xSsl;
    mbedtls_ssl_config          xConf;

#if(CFG_USER_SOC != CFG_SOC_RTL8711)

    mbedtls_entropy_context     xEntropy;
    mbedtls_ctr_drbg_context    xCtrDrbg;

#endif

}hal_tls_t;


extern hal_tls_t    *HAL_vTlsCreat(void);

extern void         HAL_vTlsClose(      hal_tls_t *pxTls);

extern int          HAL_lTlsRead(       hal_tls_t *pxTls, 
                                        char *pcBuf,            
                                        int lLen,     
                                        uint32_t ulTimeOutMs);

extern int          HAL_lTlsWrite(      hal_tls_t *pxTls, 
                                        char *pcBuf,            
                                        int lLen,     
                                        uint32_t ulTimeOutMs);

extern int          HAL_lTlsEstablish(  hal_tls_t *pxTls, 
                                        const char *pcHostname, 
                                        uint16_t usHostlen, 
                                        uint16_t usPort,              
                                        uint32_t ulTimeOutMs);

#endif


