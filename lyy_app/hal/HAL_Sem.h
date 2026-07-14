#ifndef __HAL_SEM_H__
#define __HAL_SEM_H__

#include "semphr.h"

#define SEM_TYPE            SemaphoreHandle_t

extern uint32_t lyy_semNewV2(SEM_TYPE *sem,int init_v);
extern uint8_t lyy_semWaitTryV2(SEM_TYPE sem,uint32_t time);
extern void lyy_semSignalV2(SEM_TYPE sem);
extern void lyy_semFreeV2(SEM_TYPE sem);

#endif


