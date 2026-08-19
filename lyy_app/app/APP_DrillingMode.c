/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-11-13 16:45:54
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-05 14:15:52
 * @FilePath: \lyy_app\main\app\APP_DrillingMode.c
 */
#include <stdio.h>
#include <string.h>

#include "HAL_Log.h"
#include "HAL_System.h"

#include "APP_Param.h"
#include "APP_Pluse.h"

#include "thing_mqtt.h"
#include "thing_event.h"
#include "lyy_device.h"

#include "lyy_start.h"
#include "lyy_para.h"

enum {

    APP_IDLE_ACC,
    APP_PULSE_ACC,
    APP_WWJ_ACC,
    APP_DBJ_ACC,
};


typedef struct {

    uint8_t    ucAccUpdate;
    uint8_t    ucAccType;
    APP_ACC_T *pxAcc;
    APP_ACC_T  xAcc;

}APP_DRILL_STA_T;


static APP_DRILL_STA_T g_DrillSta;


// static void APP_prvAccOp(uint32_t ulCoinIn, uint32_t ulCoinOut, uint32_t ulGiftOut, uint8_t ucAccType)
// {

//     g_DrillSta.ucAccUpdate          =  0x01;
//     g_DrillSta.ucAccType            =  ucAccType;

//     if(ulCoinIn) {

//         g_DrillSta.xAcc.ulCoinInSum     += ulCoinIn;
//         g_DrillSta.pxAcc->ulCoinInSum   += ulCoinIn;
//     }


//     if(ulCoinOut) {
        
//         g_DrillSta.xAcc.ulCoinOutSum     += ulCoinOut;
//         g_DrillSta.pxAcc->ulCoinOutSum   += ulCoinOut;
//     }

//     if(ulGiftOut) {
        
//         g_DrillSta.xAcc.ulGiftOutSum     += ulGiftOut;
//         g_DrillSta.pxAcc->ulGiftOutSum   += ulGiftOut;
//     }

// }


void APP_vCoinIn(void *pvArg)
{
    
    // APP_prvAccOp(*((uint32_t *)pvArg), 
    //              0x00, 
    //              0x00, 
    //              APP_PULSE_ACC);
    if (get_dev_type_code() == DEVICE_TYPE_PULSE)  
    {
        offline_accounts_save(*((uint32_t *)pvArg), 0, MSG_TYPE_PULSE_COIN, PAY_METHOD_OFFLINE);
    }
    else
    {
        offline_accounts_save(*((uint32_t *)pvArg), 0, MSG_TYPE_COIN_IN, PAY_METHOD_OFFLINE);  //大概率用不上
    }
    
    

    START_TYPE_LEVEL == APP_pxlGetParamePulse()->ulPulsetype
        ? APP_vPulseStart(APP_COIN_LEVEL, *((uint32_t *)pvArg))
        : APP_vPulseStart(APP_COIN_PLUSE, *((uint32_t *)pvArg));

    LYY_LOGD(">>> CoinIn sum : [%d][%d]",  g_DrillSta.pxAcc->ulCoinInSum, 
                                            *((uint32_t *)pvArg));
    
}



void APP_vGiftOut(void *pvArg)
{

    // APP_prvAccOp(0x00, 
    //              0x00, 
    //              *((uint32_t *)pvArg), 
    //              APP_PULSE_ACC);

    if (get_dev_type_code() == DEVICE_TYPE_PULSE)
    {
        offline_accounts_save(*((uint32_t *)pvArg), 0, MSG_TYPE_PULSE_GIFT, PAY_METHOD_OFFLINE);
    }
    else
    {
        offline_accounts_save(*((uint32_t *)pvArg), 0, MSG_TYPE_GIFT_OUT, PAY_METHOD_OFFLINE); // 大概率用不上
    }



    LYY_LOGD(">>> GiftOut sum : [%d][%d]",  g_DrillSta.pxAcc->ulGiftOutSum, 
                                            *((uint32_t *)pvArg));

}


void APP_vWwjCoinIn(uint32_t ulCoinIn)
{

    // APP_prvAccOp(ulCoinIn, 
    //              0x00, 
    //              0x00, 
    //              APP_WWJ_ACC);

    offline_accounts_save(ulCoinIn, 0, MSG_TYPE_COIN_IN, PAY_METHOD_OFFLINE);

    LYY_LOGD(">>> CoinIn sum : [%d][%d]",   g_DrillSta.pxAcc->ulCoinInSum, 
                                            ulCoinIn);
    
}


void APP_vWwjGiftOut(uint32_t ulGiftOut)
{

    // APP_prvAccOp(0x00, 
    //              0x00, 
    //              ulGiftOut,
    //              APP_WWJ_ACC);  
    offline_accounts_save(ulGiftOut, 0, MSG_TYPE_GIFT_OUT, PAY_METHOD_OFFLINE);

    LYY_LOGD(">>> GiftOut sum : [%d][%d]",  g_DrillSta.pxAcc->ulGiftOutSum, 
                                            ulGiftOut);

}


void APP_vDbjCoinIn(uint32_t ulCoinIn)
{

    // APP_prvAccOp(ulCoinIn, 
    //              0x00, 
    //              0x00, 
    //              APP_DBJ_ACC);

    offline_accounts_save(ulCoinIn, 0, MSG_TYPE_AMOUNT, PAY_METHOD_OFFLINE);

    LYY_LOGD(">>> CoinIn sum : [%d][%d]",   g_DrillSta.pxAcc->ulCoinInSum, 
                                            ulCoinIn);
    
}


void APP_vDbjCoinOut(uint32_t ulCoinOut)
{

    // APP_prvAccOp(0x00, 
    //              ulCoinOut, 
    //              0x00, 
    //              APP_DBJ_ACC);

    offline_accounts_save(ulCoinOut, 0, MSG_TYPE_COIN_OUT, PAY_METHOD_OFFLINE);

    LYY_LOGD(">>> CoinOut sum : [%d][%d]",  g_DrillSta.pxAcc->ulCoinOutSum, 
                                            ulCoinOut);
    
}


void APP_vAccUpdate(void)
{

    if(0x01 == g_DrillSta.ucAccUpdate) {

        APP_lSetParamUpdate();
        g_DrillSta.ucAccUpdate = 0x00;

        LYY_LOGD(">>> update acc .....");
    }

}



void APP_vReportedAcc(void)
{


    switch(g_DrillSta.ucAccType)
    {

        case APP_PULSE_ACC :{


            if(g_DrillSta.xAcc.ulCoinInSum > 0) {

                event_post_account_pulse(   g_DrillSta.xAcc.ulCoinInSum,
                                            g_DrillSta.pxAcc->ulCoinInSum,
                                            "offline_pay",
                                            NULL,
                                            "accounts_pulse_coin");

                g_DrillSta.xAcc.ulCoinInSum  = 0x00;
            }


            if(g_DrillSta.xAcc.ulGiftOutSum > 0) {

                event_post_account_pulse(   g_DrillSta.xAcc.ulGiftOutSum,
                                            g_DrillSta.pxAcc->ulGiftOutSum,
                                            "offline_pay",
                                            NULL,
                                            "accounts_pulse_gift");

                g_DrillSta.xAcc.ulGiftOutSum  = 0x00;
            }


        }break;

        case APP_WWJ_ACC   :{


            if(g_DrillSta.xAcc.ulCoinInSum > 0 || g_DrillSta.xAcc.ulGiftOutSum > 0) {

                    event_post_account_wwj_uart(   "offline_pay",
                                                    g_DrillSta.pxAcc->ulCoinInSum, 
                                                    g_DrillSta.xAcc.ulCoinInSum,
                                                    g_DrillSta.pxAcc->ulGiftOutSum, 
                                                    g_DrillSta.xAcc.ulGiftOutSum ,
                                                    NULL);

                g_DrillSta.xAcc.ulCoinInSum     = 0x00;
                g_DrillSta.xAcc.ulGiftOutSum    = 0x00;

            }


        }break;

        case APP_DBJ_ACC   :{

            if(g_DrillSta.xAcc.ulCoinInSum > 0 || g_DrillSta.xAcc.ulCoinOutSum > 0) {

                event_post_account_dbj_uart("offline_pay",
                                            g_DrillSta.pxAcc->ulCoinInSum, 
                                            g_DrillSta.xAcc.ulCoinInSum,
                                            g_DrillSta.pxAcc->ulCoinOutSum, 
                                            g_DrillSta.xAcc.ulCoinOutSum ,
                                            NULL);

                g_DrillSta.xAcc.ulCoinInSum     = 0x00;
                g_DrillSta.xAcc.ulCoinOutSum    = 0x00;
            }
    

        }break;

        default : ;

    }

    g_DrillSta.ucAccType = APP_IDLE_ACC;

}



void APP_vAccTask(void *pvArg)
{

    (void)pvArg;

    APP_vAccUpdate();

    if(lyy_mqtt_connect_result()) {

        APP_vReportedAcc();
    }
    
}


void APP_vDrillingModeInit(void)
{

    memset((void *)&g_DrillSta,
            0x00,
            sizeof(APP_DRILL_STA_T));

    g_DrillSta.pxAcc   = APP_pxlGetParameAcc();

#define APP_ACC_TASK_TIME      (4 * 1000)

    static struct HalSoftTimer AppAccTask;

    HAL_vSoftTimeInit(&AppAccTask, 
                      APP_vAccTask, 
                      APP_ACC_TASK_TIME,
                      NULL);

                      
}




