/** @file APP_Detect.h @brief 兼容旧检测接口的配网按键与 WIFI_AIRMOD 采样声明。 */
/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-10-20 11:31:27
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2023-11-17 10:37:17
 * @FilePath: \lyy_app\main\app\APP_Detect.h
 */
#ifndef __APP_DETECT_H__
#define __APP_DETECT_H__

enum 
{

    /* 配网检测 */
    APP_NETWORK_DETECT,     

    /* 投币、退礼检测 */
    APP_INPUT_GIFT,         
    APP_INPUT_COIN,         

    APP_DETECT_MAX
};


extern void     APP_vDetectInit(void);
extern void     APP_vDetecTimerMs(void);
/** @brief 轮询低有效配网键，持续 3 秒触发；继续按住则每 3 秒再次触发。 */
extern void APP_prvDetectNetWorkKey(void);
extern void     APP_vDeteclRegisterFunc(uint16_t usDetectType, void (*vFunc)(void *pvArg));

/** @brief 读取并缓存 WIFI_AIRMOD 电平，不是机台投币输出极性。 */
extern void     APP_prvDetectMachtineLvl(void);
/** @brief 返回最近缓存的配网模式电平。 @return GPIO 低/高电平对应值。 */
extern uint32_t APP_ulGetMachtineLvl(void);


#endif

