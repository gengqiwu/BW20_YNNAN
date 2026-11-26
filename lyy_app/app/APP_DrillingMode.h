#ifndef __APP_DRILLING_MODE_H__
#define __APP_DRILLING_MODE_H__

extern void APP_vCoinIn(void *pvArg);
extern void APP_vCoinInLanes(uint32_t lane1, uint32_t lane2);

extern void APP_vDbjCoinIn(uint32_t  ulCoinIn);
extern void APP_vDbjCoinOut(uint32_t ulCoinOut);

extern void APP_vWwjCoinIn(uint32_t  ulCoinIn);
extern void APP_vWwjGiftOut(uint32_t ulGiftOut);

extern void APP_vDrillingModeInit(void);

#endif


