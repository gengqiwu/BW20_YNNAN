#include <string.h>
#include <sys/param.h>


#include "lwip/netdb.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/def.h"
#include <lwip/sockets.h>
#include <lwip/inet.h>

#include "HAL_Udp.h"
#include "HAL_Log.h"




int HAL_lUdpServerCreate(const char *pcHost, uint16_t usPort)
{

    int                 lSockFd;
    int                 lOptVal  = 1;
    uint32_t            ulIp;

    struct in_addr      xInAddr;
    struct hostent      *pxHostP;
    struct sockaddr_in  xAddr;
    
    lSockFd = socket(AF_INET, SOCK_DGRAM, 0);

    if(lSockFd < 0x00) {

        LYY_LOGE("### socket create error");
        return HAL_UDP_SOCK_ERR;
    }

    if(0x00 == usPort) {

        LYY_LOGI(">>> usPort = 0x00");
        return lSockFd;
    }

    memset(&xAddr, 0, sizeof(struct sockaddr_in));

    if(0 != setsockopt(lSockFd, SOL_SOCKET, SO_REUSEADDR, &lOptVal, sizeof(lOptVal))) {

        LYY_LOGE("### setsockopt SO_REUSEADDR err");

        close(lSockFd);
        return HAL_UDP_SOCK_ERR;
    }


    if(0 != setsockopt(lSockFd, SOL_SOCKET, SO_BROADCAST, &lOptVal, sizeof(lOptVal))) {

        LYY_LOGE("### setsockopt SO_BROADCAST err");

        close(lSockFd);
        return HAL_UDP_SOCK_ERR;
    }

    if(NULL == pcHost) {

        xAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    }else {

        if(inet_aton(pcHost, &xInAddr)) {

            ulIp = *(uint32_t *)&xInAddr;
        }
        else {

            pxHostP = gethostbyname(pcHost);

            if(!pxHostP) {

                LYY_LOGE("### resolute the host address");
                close(lSockFd);
                return HAL_UDP_SOCK_ERR;
            }

            ulIp = *(uint32_t *)(pxHostP->h_addr);
        }

        xAddr.sin_addr.s_addr = ulIp;

    }


    xAddr.sin_family    = AF_INET;
    xAddr.sin_port      = htons(usPort);

    if (-1 == bind(lSockFd, (struct sockaddr *)&xAddr, sizeof(struct sockaddr_in))) {

        close(lSockFd);
        return HAL_UDP_SOCK_ERR;
    }

    LYY_LOGI(">>> success to establish udp , fd :[%d]", lSockFd);

    return lSockFd;

}





int HAL_lUdpRecvFrom(int lSockFd, HAL_NETWORK_ADDR_T *pxRemote, uint8_t *pucData, uint32_t ulDataLen, uint32_t ulTimeoutMs)
{

    int lRet;

    fd_set xReadFds;
    struct sockaddr_in xAddr;
    
    struct timeval xTimeout = {ulTimeoutMs / 1000, (ulTimeoutMs % 1000) * 1000};

    FD_ZERO(&xReadFds);
    FD_SET(lSockFd, &xReadFds);

    lRet = select(lSockFd + 1, &xReadFds, NULL, NULL, &xTimeout);

    if(0x00 == lRet) {

        return HAL_UDP_READ_TIMEOUT;
    }

    if(lRet < 0x00) {

        return HAL_UDP_READ_ERR;
    }

    socklen_t xAddrLen = sizeof(xAddr);

    lRet = recvfrom(lSockFd, 
                    pucData, 
                    ulDataLen, 
                    0, 
                    (struct sockaddr *)&xAddr, 
                    &xAddrLen);


    if(lRet > 0x00) {

        if(pxRemote) {

            pxRemote->usPort    = ntohs(xAddr.sin_port);
            strcpy((char *)pxRemote->pucAddr,  inet_ntoa(xAddr.sin_addr));

            LYY_LOGI(">>> remote host : [%s:%d]", pxRemote->pucAddr, pxRemote->usPort);
        }

        return lRet;
    }

    return HAL_UDP_READ_ERR;

}



int HAL_lUdpSendTo(int lSockFd, HAL_NETWORK_ADDR_T *pxRemote, uint8_t *pucData, uint32_t ulDataLen, uint32_t ulTimeoutMs)
{

    int                 lRet;
    uint32_t            ulIp;

    fd_set              xWriteFds;
    struct sockaddr_in  xAddr;
    struct hostent      *pxHostP;
    
    struct timeval xTimeout = {ulTimeoutMs / 1000, (ulTimeoutMs % 1000) * 1000};

    if(!pxRemote) {

        LYY_LOGE("###");
        return -1;
    }

    if(inet_aton((char *)pxRemote->pucAddr, &xAddr)) {

        ulIp = *(uint32_t *)&xAddr;

    }else {

        pxHostP = gethostbyname((char *)pxRemote->pucAddr);

        if(!pxHostP) {

            LYY_LOGE("### resolute the host address");
#if 0
            close(lSockFd);
#endif
            return HAL_UDP_SOCK_ERR;
        }

        ulIp = *(uint32_t *)(pxHostP->h_addr);
    }

    FD_ZERO(&xWriteFds);
    FD_SET(lSockFd, &xWriteFds);

    lRet = select(lSockFd + 1, NULL, &xWriteFds, NULL, &xTimeout);

    if(0x00 == lRet) {

        return HAL_UDP_WRITE_TIMEOUT;
    }


    if(lRet < 0x00) {

        return HAL_UDP_WRITE_ERR;
    }

    xAddr.sin_family        = AF_INET;
    xAddr.sin_addr.s_addr   = ulIp;
    xAddr.sin_port          = htons(pxRemote->usPort);


    lRet = sendto(  lSockFd, 
                    pucData, 
                    ulDataLen, 
                    0, 
                    (struct sockaddr *)&xAddr, 
                    sizeof(struct sockaddr_in));

    LYY_LOGD(">>> upd send to : [%d]", lRet);

    return lRet > 0x00 ? lRet : HAL_UDP_WRITE_ERR;

}






