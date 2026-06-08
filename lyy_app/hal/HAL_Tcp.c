#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "lwip/netdb.h"
#include "lwip/opt.h"
#include "lwip/sys.h"
#include "lwip/api.h"
#include "lwip/def.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"


#include "HAL_Log.h"
#include "HAL_System.h"


static uint64_t HAL_prvTimeLeft(uint64_t ulEnd, uint64_t ulNow)
{
    uint64_t uLeft;

    if (ulEnd > ulNow) {

        uLeft = ulEnd - ulNow;
    } else {

        uLeft = 0;
    }

    return uLeft;
}


uint32_t HAL_ulTcpEstablish(const char *pHost, uint16_t usPort)
{
    char   pcProtStr[6]  = {0x00};
    struct addrinfo xHints, *pxAddrInfoList = NULL, *pxCur = NULL;

    memset(&xHints, 0, sizeof(struct addrinfo));

    /* ipv4 */
    xHints.ai_family   = AF_INET; 
    xHints.ai_socktype = SOCK_STREAM;
    xHints.ai_protocol = IPPROTO_TCP;

    sprintf((char *)pcProtStr, "%u", usPort);

    /* 域名解析 */
    if (getaddrinfo((const char *)pHost, (char *)pcProtStr, &xHints, &pxAddrInfoList) != 0) {

        LYY_LOGE("getaddrinfo error");
        return 0;
    }

    int32_t lFd, lRc = 0, lSockopt = 1;

    pxCur = pxAddrInfoList;

    for (; pxCur != NULL; pxCur = pxCur->ai_next) {

        if(pxCur->ai_family != AF_INET) {

            lRc = 0;
            continue;
        }

        lFd = socket(pxCur->ai_family, pxCur->ai_socktype, pxCur->ai_protocol);
        if (lFd < 0) {

            lRc = 0;
            continue;
        }

        setsockopt(lFd, SOL_SOCKET,  SO_REUSEADDR, &lSockopt, sizeof(lSockopt));
        setsockopt(lFd, SOL_SOCKET,  SO_REUSEPORT, &lSockopt, sizeof(lSockopt));

        /* 12K(tcp 8K) */
        int lBuffSize = (3 << 12);                                                   

        setsockopt(lFd, SOL_SOCKET, SO_SNDBUF,  &lBuffSize, sizeof(int));
        setsockopt(lFd, SOL_SOCKET, SO_RCVBUF,  &lBuffSize, sizeof(int));

        /* 网络游戏，禁止nagle算法 */
        setsockopt(lFd, IPPROTO_TCP, TCP_NODELAY,  &lSockopt, sizeof(lSockopt));

#if defined(__linux__)
        /* linux环境下防止断开后，禁止 send() 函数向系统发送常消息。 */
        setsockopt(lFd, SOL_SOCKET, MSG_NOSIGNAL,  &lSockopt, sizeof(lSockopt));
#endif

        if (connect(lFd, pxCur->ai_addr, pxCur->ai_addrlen) == 0) {

            lRc = lFd;
            break;
        }

        close(lFd);
        lRc = 0;
    }

    freeaddrinfo(pxAddrInfoList);

    return lRc;
}


int32_t HAL_uTcpServerAccept(uint32_t lFd) 
{
	struct sockaddr_in xServerAddr;
    socklen_t xSockLen = sizeof(xServerAddr);

    return accept(lFd, (struct sockaddr *)&xServerAddr, &xSockLen);
}


/* 定制服务器监听接口 */
uint32_t HAL_uTcpServerListen(const char * pcIp, uint16_t usPort)
{
    int32_t lFd, lSockopt = 1;

    lFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (lFd < 0) {

        return 0x00;
    }

    struct sockaddr_in xServerAddr;
	memset(&xServerAddr, 0x00, sizeof(xServerAddr));

    xServerAddr.sin_family      = AF_INET;
    xServerAddr.sin_port        = htons(usPort);
    xServerAddr.sin_addr.s_addr = NULL == pcIp ? INADDR_ANY : inet_addr(pcIp);


    setsockopt(lFd, SOL_SOCKET,  SO_REUSEADDR, &lSockopt, sizeof(lSockopt));
    setsockopt(lFd, SOL_SOCKET,  SO_REUSEPORT, &lSockopt, sizeof(lSockopt));

    if(bind(lFd, (struct sockaddr *)&xServerAddr, sizeof(xServerAddr)) < 0)
    {
        LYY_LOGE("can not bind socket");
        close(lFd);

        return 0x00;
    }

    /*  */
    listen(lFd, 1); 

    return lFd;
}


uint32_t HAL_ulTcpDestroy(uint32_t ulFd)
{

#if 0   
    /* 由于缓冲区数据没丢完会导致失败，导致没有close到 */
    if (0 != shutdown((int) ulFd, SHUT_RDWR)) {
        return 0;
    }
#else
    shutdown((int) ulFd, SHUT_RDWR);
#endif

    if (0 != close((int) ulFd)) {

        return 0;
    }

    LYY_LOGD("=============== tcp close sock =================");

    return 1;
} 


uint32_t HAL_ulTcpWrite(uint32_t ulFd, const char *pcBuf, uint32_t ulLen, uint32_t ulTimeoutMs)
{
    fd_set   xSets;

    int      lRet      = 1;
    uint32_t ulLenSent = 0;
    uint64_t ulEnd, ulLeft;

    ulEnd = HAL_ulGetTimeMs() + ulTimeoutMs;

    do {

        ulLeft = HAL_prvTimeLeft(ulEnd, HAL_ulGetTimeMs());

        if (0 != ulLeft) {

            struct timeval xTimeout;

            FD_ZERO(&xSets);
            FD_SET(ulFd, &xSets);

            xTimeout.tv_sec =  ulLeft / 1000;
            xTimeout.tv_usec = (ulLeft % 1000) * 1000;
            

            lRet = select(ulFd + 1, NULL, &xSets, NULL, &xTimeout);

            if (lRet > 0) {

                if (0 == FD_ISSET(ulFd, &xSets)) {

                    lRet = 0;
                    continue;
                }
            } else if (0 == lRet) {

                LYY_LOGE("select-write timeout %d \r\n", (int)ulFd);
                break;
            } else {

                if (EINTR == errno) {

                    LYY_LOGE("EINTR be caught \r\n");
                    continue;
                }

                break;
            }
        }

        if (lRet > 0) {

            lRet = send(ulFd, pcBuf + ulLenSent, ulLen - ulLenSent, 0);

            if (lRet > 0) {

                ulLenSent += lRet;

            } else if (0 == lRet) {

                LYY_LOGE( "No data be sent \r\n");
            } else {

                if (EINTR == errno) {

                    LYY_LOGE("EINTR be caught \r\n");
                    continue;
                }

                LYY_LOGE( "send fail \r\n");
                break;
            }
        }
    } while ((ulLenSent < ulLen) && (HAL_prvTimeLeft(ulEnd, HAL_ulGetTimeMs()) > 0));

    return ulLenSent;    
}


int HAL_lTcpRead(uint32_t ulFd, char *pcBuf, uint32_t ulLen, uint32_t ulTimeoutMs)
{
    fd_set   xSets;

    int lRet, lErrCode  = 0x00;
    uint32_t  ulLenRecv = 0x00;
    uint64_t  ulEnd, ulLeft;

    ulEnd = HAL_ulGetTimeMs() + ulTimeoutMs;

    do {

        ulLeft = HAL_prvTimeLeft(ulEnd, HAL_ulGetTimeMs());
        if (0 == ulLeft) {
            break;
        }

        struct timeval timeout;

        FD_ZERO(&xSets);
        FD_SET(ulFd, &xSets);

        timeout.tv_sec = ulLeft / 1000;
        timeout.tv_usec = (ulLeft % 1000) * 1000;

        lRet = select(ulFd + 1, &xSets, NULL, NULL, &timeout);
        if (lRet > 0) {

            lRet = recv(ulFd, pcBuf + ulLenRecv, ulLen - ulLenRecv, 0);

            if (lRet > 0) {

                ulLenRecv += lRet;
            } else if (0 == lRet) {

                LYY_LOGE("connection is closed \r\n");
                lErrCode = -1;
                break;
            } else {

                if (EINTR == errno) {

                    LYY_LOGE( "EINTR be caught \r\n");
                    continue;
                }

                lErrCode = -2;
                break;
            }
        } else if (0 == lRet) {

            break;
        } else {

            LYY_LOGE("select-recv fail \r\n");
            lErrCode = -2;
            break;
        }
    } while ((ulLenRecv < ulLen));


    return (0 != ulLenRecv) ? (int)ulLenRecv : lErrCode;    
}


