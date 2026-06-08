#ifndef __APP_PLUSE_H__
#define __APP_PLUSE_H__

enum {

    APP_COIN_PLUSE,             
    APP_COIN_LEVEL, 
    APP_PULSE_MAX
};


extern void APP_vPulseimerMs(void);
extern void APP_vPulseStart(uint32_t ulPulseType, uint32_t ulPulseCnt);

#endif


