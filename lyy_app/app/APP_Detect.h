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
extern void APP_prvDetectNetWorkKey(void);
extern void     APP_vDeteclRegisterFunc(uint16_t usDetectType, void (*vFunc)(void *pvArg));

extern void     APP_prvDetectMachtineLvl(void);
extern uint32_t APP_ulGetMachtineLvl(void);


#endif

