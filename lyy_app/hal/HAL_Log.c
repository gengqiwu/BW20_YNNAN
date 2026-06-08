/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-10-11 16:31:38
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-06 20:22:05
 * @FilePath: \ESP8266_RTOS_SDK\lyy_app\main\hal\hal_log.c
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "HAL_System.h"

#include "HAL_Log.h"
#include "hal_uart.h"

#define HAL_XLOG_BUFF_SIZE_MAX   	(512U)

static char g_cDbgBuf[HAL_XLOG_BUFF_SIZE_MAX] = {0x00, };

void HAL_vLogWrite(const char *cFormat, ...)
{

	memset(g_cDbgBuf, 0x00, HAL_XLOG_BUFF_SIZE_MAX);
	
	va_list xArg;
	va_start(xArg, cFormat);
	vsnprintf(g_cDbgBuf, HAL_XLOG_BUFF_SIZE_MAX - 1, cFormat, xArg);

	printf("%s", g_cDbgBuf);

	va_end(xArg);

}

/**
 * @brief
 * hex 打印
 * @param phex
 * @param len
 */
void lyy_printf_hex(uint8_t *phex, uint16_t len)
{
#define PRINTF_LEN_MAX 2000
	if (NULL == phex || 0x00 == len)
	{
		return;
	}

	uint16_t i, j;
	uint8_t const printf_char[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
	char *pbuffer = NULL;
	uint16_t len_string = len * 2 + 1;
	if (len_string >= PRINTF_LEN_MAX)
	{
		LYY_LOGD("len is too long  . len:%d", len_string);
		return;
	}
	pbuffer = malloc(len_string); // 增加一个结束符
	if (pbuffer == NULL)
		return;
	memset(pbuffer, 0, len_string);
	for (i = 0, j = 0; j < len; j++)
	{
		pbuffer[i++] = printf_char[phex[j] >> 4];
		pbuffer[i++] = printf_char[phex[j] & 0x0f];
	}
	pbuffer[len_string - 1] = '\0';
	LYY_LOGD("%s", pbuffer);

	free(pbuffer);
}
