#include <stdio.h>

#include "HAL_Log.h"
#include "HAL_Gpio.h"
#include "HAL_System.h"

#include "APP_Param.h"
#include "APP_Pluse.h"
#include "APP_Detect.h"

enum {

    ePluseOpen,
    ePluseClose,
};


typedef struct {

    uint32_t    usPulseCnt;             /* 启动次数 */
	uint32_t    usTimerCnt;             /* 记时 */
    uint32_t    usTriggerCond;          /* 触发时间条件 */
    uint32_t    ulLogo;                 /* 触发标志 */
	void	    (*vFunc)(void);
}APP_PULSE_STA_T;


static void APP_prvPulseStartOp(void);
static void APP_prvLevelStartOp(void);


#define APP_COIN_OUT_OPEN                   HAL_vGpioSet(GPIO_COIN_OUT,   !APP_ulGetMachtineLvl() )
#define APP_COIN_OUT_CLOSE                  HAL_vGpioSet(GPIO_COIN_OUT,   APP_ulGetMachtineLvl()  )


static APP_PULSE_STA_T g_xPulseInfo[] =     {   {   0x00, 
                                                    0x00, 
                                                    0x00, 
                                                    0x00,
                                                    APP_prvPulseStartOp},

                                                {   0x00, 
                                                    0x00, 
                                                    0x00, 
                                                    0x00,
                                                    APP_prvLevelStartOp}, 
                                            };



void APP_vPulseStart(uint32_t ulPulseType, uint32_t ulPulseCnt)
{

    switch(ulPulseType)
    {
        case APP_COIN_PLUSE : {

            if( 0x00 == g_xPulseInfo[APP_COIN_PLUSE].usTimerCnt && 
                0x00 == g_xPulseInfo[APP_COIN_PLUSE].usTriggerCond) {

                APP_PULSE_T *pxPulse = APP_pxlGetParamePulse();

                g_xPulseInfo[APP_COIN_PLUSE].usTriggerCond  = pxPulse->ulPulseWidth; 
                g_xPulseInfo[APP_COIN_PLUSE].ulLogo         = ePluseOpen;

                APP_COIN_OUT_OPEN;
            }

            g_xPulseInfo[APP_COIN_PLUSE].usPulseCnt += ulPulseCnt;   

        }break;

        case APP_COIN_LEVEL : {

#define APP_LVL_TIME_MIN       (60U * 1000U)

            g_xPulseInfo[APP_COIN_LEVEL].usTriggerCond += ulPulseCnt * APP_LVL_TIME_MIN; 

            APP_COIN_OUT_OPEN;

        }break;

        default : break;

    }

}


static void APP_prvLevelStartOp(void)
{

    g_xPulseInfo[APP_COIN_LEVEL].usTriggerCond = 0; 

    APP_COIN_OUT_CLOSE;

}


static void APP_prvPulseStartOp(void)
{

    APP_PULSE_T *pxPulse = APP_pxlGetParamePulse();

    if(ePluseOpen == g_xPulseInfo[APP_COIN_PLUSE].ulLogo) {

        g_xPulseInfo[APP_COIN_PLUSE].usTriggerCond = 
                        0x00 == --g_xPulseInfo[APP_COIN_PLUSE].usPulseCnt ? 0x00 : pxPulse->ulPulseSpace;

        g_xPulseInfo[APP_COIN_PLUSE].ulLogo         = ePluseClose;

        APP_COIN_OUT_CLOSE;

    }
    else {

        g_xPulseInfo[APP_COIN_PLUSE].usTriggerCond  = pxPulse->ulPulseWidth;
        g_xPulseInfo[APP_COIN_PLUSE].ulLogo         = ePluseOpen;

        APP_COIN_OUT_OPEN;
        
    }

}


void APP_vPulseimerMs(void)
{ 

    uint8_t ucCnt = 0x00;

    for( ; ucCnt < APP_PULSE_MAX; ucCnt++) {

        if( g_xPulseInfo[ucCnt].usTriggerCond != 0x00 && 
            ++g_xPulseInfo[ucCnt].usTimerCnt == g_xPulseInfo[ucCnt].usTriggerCond) {

            if(g_xPulseInfo[ucCnt].vFunc)  {

                g_xPulseInfo[ucCnt].vFunc();
            }

            g_xPulseInfo[ucCnt].usTimerCnt = 0x00;

        }
    }
   
}








