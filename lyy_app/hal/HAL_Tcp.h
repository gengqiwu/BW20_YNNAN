#ifndef __HAL_TCP_H__
#define __HAL_TCP_H__

#include <stdint.h>

extern uint32_t HAL_ulTcpDestroy(uint32_t ulFd);
extern uint32_t HAL_ulTcpEstablish(const char *pHost, uint16_t usPort);
extern int      HAL_lTcpRead(uint32_t ulFd, char *pcBuf, uint32_t ulLen, uint32_t ulTimeoutMs);
extern uint32_t HAL_ulTcpWrite(uint32_t ulFd, const char *pcBuf, uint32_t ulLen, uint32_t ulTimeoutMs);

extern int32_t  HAL_uTcpServerAccept(uint32_t lFd);
extern uint32_t HAL_uTcpServerListen(const char * pcIp, uint16_t usPort);

#endif

