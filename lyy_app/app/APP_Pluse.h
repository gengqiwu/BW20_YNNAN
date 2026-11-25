#ifndef __APP_PLUSE_H__
#define __APP_PLUSE_H__

#include <stdint.h>

enum {

    APP_COIN_PLUSE,             
    APP_COIN_LEVEL, 
    APP_PULSE_MAX
};


extern void APP_vPulseimerMs(void);
extern void APP_vPulseStart(uint32_t ulPulseType, uint32_t ulPulseCnt);
/* Called after MODE_CTRL detection, before the pulse timer is started. */
extern void APP_vPulseOutputInit(uint8_t bDualLane);
/* Queue a pulse-only request; lane uses the start queue's zero-based x. */
extern uint8_t APP_bTryLanePulseStart(uint16_t usLane, uint32_t ulPulseCnt);
/* Kept for the existing GIFT output API; uses the initialized idle polarity. */
extern void APP_vGiftPulseStart(uint32_t ulPulseCnt);

#endif


