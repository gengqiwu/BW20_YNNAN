/**
 * @file APP_DrillingMode.h
 * @brief 声明账目初始化及入币、出币、出礼数量回调接口。
 */
#ifndef __APP_DRILLING_MODE_H__
#define __APP_DRILLING_MODE_H__

extern void APP_vCoinIn(void *pvArg);
extern void APP_vGiftOut(void *pvArg);

/**
 * @brief 累加兑币机入币账目并标记该类型待报。
 * @param[in] pvArg 指向有效 uint32_t 数量；先调用账目初始化，不转移所有权。
 */
extern void APP_vDbjCoinIn(void *pvArg);
/**
 * @brief 累加兑币机出币账目并标记周期保存及上报。
 * @param[in] pvArg 指向有效 uint32_t 数量；先调用账目初始化，不转移所有权。
 */
extern void APP_vDbjCoinOut(void *pvArg);

/**
 * @brief 累加娃娃机入币账目并标记该类型待报。
 * @param[in] pvArg 指向有效 uint32_t 数量；先调用账目初始化，不转移所有权。
 */
extern void APP_vWwjCoinIn(void *pvArg);
/**
 * @brief 累加娃娃机出礼账目并标记该类型待报。
 * @param[in] pvArg 指向有效 uint32_t 数量；先调用账目初始化，不转移所有权。
 */
extern void APP_vWwjGiftOut(void *pvArg);

extern void APP_vDrillingModeInit(void);

#endif


