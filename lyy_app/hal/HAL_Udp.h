#ifndef __HAL_UDP_H__
#define __HAL_UDP_H__


enum {

    HAL_UDP_WRITE_ERR       = -6,
    HAL_UDP_WRITE_TIMEOUT   = -5,
    HAL_UDP_READ_ERR        = -4,
    HAL_UDP_READ_TIMEOUT    = -3,
    HAL_UDP_PARAM_ERR       = -2,
    HAL_UDP_SOCK_ERR        = -1,

    HAL_UDP_SUCCESS         = 0x00

    
};

typedef struct {


    uint16_t usPort;
    uint8_t  pucAddr[16];
}HAL_NETWORK_ADDR_T;

extern int HAL_lUdpServerCreate(const char *pcHost, uint16_t usPort);
extern int HAL_lUdpSendTo(int lSockFd, HAL_NETWORK_ADDR_T *pxRemote, uint8_t *pucData, uint32_t ulDataLen, uint32_t ulTimeoutMs);
extern int HAL_lUdpRecvFrom(int lSockFd, HAL_NETWORK_ADDR_T *pxRemote, uint8_t *pucData, uint32_t ulDataLen, uint32_t ulTimeoutMs);

#endif

