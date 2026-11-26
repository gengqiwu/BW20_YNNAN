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

#define ACC_TIME_INTERRUPT_MS (50U)     // 定时器周期
#define ACC_PERIOD_FOR_CHECKSTA (5000U) // 获取设备状态的周期
static int acc_timeout_to_checkSta = ACC_PERIOD_FOR_CHECKSTA / ACC_TIME_INTERRUPT_MS;

typedef struct {

    uint32_t    ucAccCoinIn;
    uint32_t    ucAccGiftOut;
}APP_ACC_UPDATE_T;

static APP_ACC_UPDATE_T app_acc_update_t;
static uint32_t g_aAccCoinLanes[2];
static uint32_t g_aPendingCoinPulses[2];

// enum {

//     APP_IDLE_ACC,
//     APP_PULSE_ACC,
//     APP_WWJ_ACC,
//     APP_DBJ_ACC,
// };


// typedef struct {

//     uint8_t    ucAccUpdate;
//     uint8_t    ucAccType;
//     APP_ACC_T *pxAcc;
//     APP_ACC_T  xAcc;

// }APP_DRILL_STA_T;


// static APP_DRILL_STA_T g_DrillSta;



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


/**
 * @brief 保持旧机型的实体投币转发行为。
 * @param count 有效投币脉冲数，按原配置以脉冲或持续电平输出。
 */
static void APP_prvForwardCoin(uint32_t count)
{
    APP_vPulseStart(START_TYPE_LEVEL == APP_pxlGetParamePulse()->ulPulsetype ?
                    APP_COIN_LEVEL : APP_COIN_PLUSE, count);
}

/**
 * @brief 分别向 COIN_OUT 和 GIFT_OUT 的队列提交投币脉冲。
 * @pre 调用者持有任务临界区，防止重试和新投币重复提交同一批计数。
 * @note 未被队列接收的计数留在原货道，下次账目任务再尝试。
 */
static void APP_prvForwardCoinLanes(void)
{
    for (uint16_t lane = 0; lane < 2; ++lane) {
        uint32_t pending = g_aPendingCoinPulses[lane];
        if (pending && APP_bTryLanePulseStart(lane, pending)) {
            g_aPendingCoinPulses[lane] = 0;
        }
    }
}

/**
 * @brief 接收已归属货道的投币计数，分路记账并按 1:1 转发。
 * @param lane1 货道 1 的有效投币脉冲数，输出到 COIN_OUT。
 * @param lane2 货道 2 的有效投币脉冲数，输出到 GIFT_OUT。
 * @note 双货道实体投币只使用脉冲输出，不受旧版持续电平模式影响。
 */
void APP_vCoinInLanes(uint32_t lane1, uint32_t lane2)
{
    taskENTER_CRITICAL();
    g_aAccCoinLanes[0] += lane1;
    g_aAccCoinLanes[1] += lane2;
    g_aPendingCoinPulses[0] += lane1;
    g_aPendingCoinPulses[1] += lane2;
    APP_prvForwardCoinLanes();
    acc_timeout_to_checkSta = ACC_PERIOD_FOR_CHECKSTA / ACC_TIME_INTERRUPT_MS;
    taskEXIT_CRITICAL();
}

void APP_vCoinIn(void *pvArg)
{
    
    // APP_prvAccOp(*((uint32_t *)pvArg), 
    //              0x00, 
    //              0x00, 
    //              APP_PULSE_ACC);

    acc_timeout_to_checkSta = ACC_PERIOD_FOR_CHECKSTA / ACC_TIME_INTERRUPT_MS;
    app_acc_update_t.ucAccCoinIn += *((uint32_t *)pvArg);
    

    APP_prvForwardCoin(*((uint32_t *)pvArg));

    //LYY_LOGD(">>> CoinIn sum : [%d][%d]",  g_DrillSta.pxAcc->ulCoinInSum, 
                                            //*((uint32_t *)pvArg));
    
}



void APP_vWwjCoinIn(uint32_t ulCoinIn)
{

    // APP_prvAccOp(ulCoinIn, 
    //              0x00, 
    //              0x00, 
    //              APP_WWJ_ACC);

    offline_accounts_save(ulCoinIn, 0, MSG_TYPE_COIN_IN, PAY_METHOD_OFFLINE);

    //LYY_LOGD(">>> CoinIn sum : [%d][%d]",   g_DrillSta.pxAcc->ulCoinInSum, 
                                            //ulCoinIn);
    
}


void APP_vWwjGiftOut(uint32_t ulGiftOut)
{

    // APP_prvAccOp(0x00, 
    //              0x00, 
    //              ulGiftOut,
    //              APP_WWJ_ACC);  
    offline_accounts_save(ulGiftOut, 0, MSG_TYPE_GIFT_OUT, PAY_METHOD_OFFLINE);

    //LYY_LOGD(">>> GiftOut sum : [%d][%d]",  g_DrillSta.pxAcc->ulGiftOutSum, 
                                            //ulGiftOut);

}


void APP_vDbjCoinIn(uint32_t ulCoinIn)
{

    // APP_prvAccOp(ulCoinIn, 
    //              0x00, 
    //              0x00, 
    //              APP_DBJ_ACC);

    offline_accounts_save(ulCoinIn, 0, MSG_TYPE_AMOUNT, PAY_METHOD_OFFLINE);

    //LYY_LOGD(">>> CoinIn sum : [%d][%d]",   g_DrillSta.pxAcc->ulCoinInSum, 
                                            // ulCoinIn);
    
}


void APP_vDbjCoinOut(uint32_t ulCoinOut)
{

    // APP_prvAccOp(0x00, 
    //              ulCoinOut, 
    //              0x00, 
    //              APP_DBJ_ACC);

    offline_accounts_save(ulCoinOut, 0, MSG_TYPE_COIN_OUT, PAY_METHOD_OFFLINE);

    //LYY_LOGD(">>> CoinOut sum : [%d][%d]",  g_DrillSta.pxAcc->ulCoinOutSum, 
                                            //ulCoinOut);
    
}


// void APP_vAccUpdate(void)
// {

//     if(0x01 == g_DrillSta.ucAccUpdate) {

//         //APP_lSetParamUpdate();  //不在保存


//         g_DrillSta.ucAccUpdate = 0x00;

//         LYY_LOGD(">>> update acc .....");
//     }

// }



// void APP_vReportedAcc(void)
// {


//     switch(g_DrillSta.ucAccType)
//     {

//         case APP_PULSE_ACC :{


//             if(g_DrillSta.xAcc.ulCoinInSum > 0) {

//                 event_post_account_pulse(   g_DrillSta.xAcc.ulCoinInSum,
//                                             g_DrillSta.pxAcc->ulCoinInSum,
//                                             "offline_pay",
//                                             NULL,
//                                             "accounts_pulse_coin");

//                 g_DrillSta.xAcc.ulCoinInSum  = 0x00;
//             }


//             if(g_DrillSta.xAcc.ulGiftOutSum > 0) {

//                 event_post_account_pulse(   g_DrillSta.xAcc.ulGiftOutSum,
//                                             g_DrillSta.pxAcc->ulGiftOutSum,
//                                             "offline_pay",
//                                             NULL,
//                                             "accounts_pulse_gift");

//                 g_DrillSta.xAcc.ulGiftOutSum  = 0x00;
//             }


//         }break;

//         case APP_WWJ_ACC   :{


//             if(g_DrillSta.xAcc.ulCoinInSum > 0 || g_DrillSta.xAcc.ulGiftOutSum > 0) {

//                     event_post_account_wwj_uart(   "offline_pay",
//                                                     g_DrillSta.pxAcc->ulCoinInSum, 
//                                                     g_DrillSta.xAcc.ulCoinInSum,
//                                                     g_DrillSta.pxAcc->ulGiftOutSum, 
//                                                     g_DrillSta.xAcc.ulGiftOutSum ,
//                                                     NULL);

//                 g_DrillSta.xAcc.ulCoinInSum     = 0x00;
//                 g_DrillSta.xAcc.ulGiftOutSum    = 0x00;

//             }


//         }break;

//         case APP_DBJ_ACC   :{

//             if(g_DrillSta.xAcc.ulCoinInSum > 0 || g_DrillSta.xAcc.ulCoinOutSum > 0) {

//                 event_post_account_dbj_uart("offline_pay",
//                                             g_DrillSta.pxAcc->ulCoinInSum, 
//                                             g_DrillSta.xAcc.ulCoinInSum,
//                                             g_DrillSta.pxAcc->ulCoinOutSum, 
//                                             g_DrillSta.xAcc.ulCoinOutSum ,
//                                             NULL);

//                 g_DrillSta.xAcc.ulCoinInSum     = 0x00;
//                 g_DrillSta.xAcc.ulCoinOutSum    = 0x00;
//             }
    

//         }break;

//         default : ;

//     }

//     g_DrillSta.ucAccType = APP_IDLE_ACC;

// }



/**
 * @brief 定期重试待转发脉冲，并将已累计投币写入离线账目缓存。
 * @param pvArg 定时器参数，未使用。
 * @details 双货道按 position_x=1 或 2 分别写账，position_y 固定为 0。
 *          此处不重新读取屏幕页面，避免切页改变投币归属。
 */
void APP_vAccTask(void *pvArg)   //新线下账目优化后
{

    (void)pvArg;

    uint32_t lane1 = 0, lane2 = 0;
    taskENTER_CRITICAL();
    APP_prvForwardCoinLanes();
    if (acc_timeout_to_checkSta) --acc_timeout_to_checkSta;
    int flush = (acc_timeout_to_checkSta == 0);
    if (flush) {
        lane1 = g_aAccCoinLanes[0];
        lane2 = g_aAccCoinLanes[1];
        g_aAccCoinLanes[0] = g_aAccCoinLanes[1] = 0;
        acc_timeout_to_checkSta = ACC_PERIOD_FOR_CHECKSTA / ACC_TIME_INTERRUPT_MS;
    }
    taskEXIT_CRITICAL();
    if (flush)
    {
        if (lane1) offline_accounts_save_ndj(lane1, 0, 1, MSG_TYPE_COIN_IN, PAY_METHOD_OFFLINE);
        if (lane2) offline_accounts_save_ndj(lane2, 0, 2, MSG_TYPE_COIN_IN, PAY_METHOD_OFFLINE);
        //event_post_log("acc_time_test");

        if (app_acc_update_t.ucAccCoinIn != 0)
        {

            if (get_dev_type_code() == DEVICE_TYPE_PULSE)
            {
                offline_accounts_save(app_acc_update_t.ucAccCoinIn, 0, MSG_TYPE_PULSE_COIN, PAY_METHOD_OFFLINE);
            }
            else
            {
                offline_accounts_save(app_acc_update_t.ucAccCoinIn, 0, MSG_TYPE_COIN_IN, PAY_METHOD_OFFLINE); // 大概率用不上
            }

            app_acc_update_t.ucAccCoinIn = 0; //账目清零
        }

        if (app_acc_update_t.ucAccGiftOut != 0)
        {
            if (get_dev_type_code() == DEVICE_TYPE_PULSE)
            {
                offline_accounts_save(app_acc_update_t.ucAccGiftOut, 0, MSG_TYPE_PULSE_GIFT, PAY_METHOD_OFFLINE);
            }
            else
            {
                offline_accounts_save(app_acc_update_t.ucAccGiftOut, 0, MSG_TYPE_GIFT_OUT, PAY_METHOD_OFFLINE); // 大概率用不上
            }
            app_acc_update_t.ucAccGiftOut = 0; // 账目清零
        }
    }
}


void APP_vDrillingModeInit(void)
{

    // memset((void *)&g_DrillSta,
    //         0x00,
    //         sizeof(APP_DRILL_STA_T));

    //g_DrillSta.pxAcc   = APP_pxlGetParameAcc();

    static struct HalSoftTimer AppAccTask;

    HAL_vSoftTimeInit(&AppAccTask,
                      APP_vAccTask,
                      ACC_TIME_INTERRUPT_MS,
                      NULL);
}




