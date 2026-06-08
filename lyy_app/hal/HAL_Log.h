/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-10-11 16:31:36
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-06-27 15:11:24
 * @FilePath: \ESP8266_RTOS_SDK\lyy_app\main\hal\hal_log.h
 */

#ifndef __HAL_LOG_H__
#define __HAL_LOG_H__

#include "USER_Config.h"

#ifdef CFG_RELEASE
#define XLOG_LEVEL     		XLOG_NONE	        // 打印选择
#else
#define XLOG_LEVEL     		XLOG_VERBOSE	        // 打印选择
#endif

typedef enum 
{
    XLOG_NONE,       /*!< No log output */
    XLOG_ERROR,      /*!< Critical errors, software module can not recover on its own */
    XLOG_WARN,       /*!< Error conditions from which recovery measures have been taken */
    XLOG_INFO,       /*!< Information messages which describe normal flow of events */
    XLOG_DEBUG,      /*!< Extra information which is not necessary for normal use (values, pointers, sizes, etc). */
    XLOG_VERBOSE     /*!< Bigger chunks of debugging information, or frequent messages which can potentially flood the output. */
	
} eLogLevel;

extern void HAL_vLogUartSwipInit(void);
extern void HAL_vLogWrite(const char *cFormat, ...);
extern void lyy_printf_hex(uint8_t *phex, uint16_t len);

#ifdef XLOG_COLOR
#define LOG_FORMAT(letter, format)  LOG_COLOR_ ## letter #letter " [%s, %s, %d]: " format LOG_RESET_COLOR "\n"
#else
#define LOG_FORMAT(letter, format)  #letter " [%s, %s, %d]: " format "\n"
#endif


#define LYY_LOGE( format, ... ) do { \
        if (XLOG_LEVEL >= XLOG_ERROR) { \
            HAL_vLogWrite(LOG_FORMAT(E, format), __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define LYY_LOGW( format, ... ) do { \
        if (XLOG_LEVEL >= XLOG_WARN) { \
            HAL_vLogWrite(LOG_FORMAT(W, format), __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define LYY_LOGI( format, ... ) do { \
        if (XLOG_LEVEL >= XLOG_INFO) { \
            HAL_vLogWrite(LOG_FORMAT(I, format), __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define LYY_LOGD( format, ... ) do { \
        if (XLOG_LEVEL >= XLOG_DEBUG) { \
            HAL_vLogWrite(LOG_FORMAT(D, format), __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)

#define LYY_LOGV( format, ... ) do { \
        if (XLOG_LEVEL >= XLOG_VERBOSE) { \
            HAL_vLogWrite(LOG_FORMAT(V, format), __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
        } \
    } while(0)



#endif

