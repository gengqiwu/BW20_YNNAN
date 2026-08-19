#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "hal_sem.h"

/**
 * @brief 
 * 配置信号量申请函数V2
 * @param init_v  支持0/1  
 * @return lyy_uint32_t >0  成功，表示信号量I   =0 失败 
 */
uint32_t lyy_semNewV2(SEM_TYPE *sem,int init_v)
{
	(void)init_v;
	if (*sem == NULL)                    // 仅首次创建，避免每次调用都新建导致内存泄漏
		*sem = xSemaphoreCreateBinary(); // 创建二值信号量
	if (*sem != NULL)
		xSemaphoreTake(*sem, 0);         // 清除可能残留的信号，确保起始为空
	return 0;
}

/**
 * @brief 
 * 配置信号量减一函数
 * @param semid 信号量ID
 * @param time 超时时间，单位为ms 
 * @return lyy_uint8_t >0 获取成功 ，=0 超时 
 */
uint8_t lyy_semWaitTryV2(SEM_TYPE sem,uint32_t time)
{
	if(sem == NULL)
		return 0;

	if (!xSemaphoreTake(sem, time)) 
	{
		return 0;
	}
	return 1;
}
/**
 * @brief 
 * 配置信号量释放函数
 * @param semid 信号量ID
 */
void lyy_semSignalV2(SEM_TYPE sem)
{
	if(sem == NULL)//信号量未初始化
		return;
	xSemaphoreGive(sem);
}
/**
 * @brief 
 * 配置信号量释放函数
 * @param semid 
 */
void lyy_semFreeV2(SEM_TYPE sem)
{
	if(sem == NULL)//信号量未初始化
		return;
	xSemaphoreGive(sem); //
}




