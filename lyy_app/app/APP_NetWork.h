/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-11-26 16:21:46
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-11-28 10:46:51
 * @FilePath: \ameba-rtos\lyy_app\app\APP_NetWork.h
 */
#ifndef __APP_NETWORK_H__
#define __APP_NETWORK_H__

typedef enum {
    
    CLIENT_DIS_CON,
    CLIENT_CONNING,
    CLIENT_CONED,
    CLIENT_ISPAUSED
}APP_CLIENT_STA_T;


extern void             APP_vNetWorkInit(void);
extern APP_CLIENT_STA_T APP_pxGetClientSta(void);
extern int              APP_lClientWrite(char *pcBuf, int lLen);
extern void             APP_pxSetClientSta(APP_CLIENT_STA_T xSta);
extern void             APP_prvClientCloseSocket(void);



#endif

