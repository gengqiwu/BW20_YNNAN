#include <stdio.h>
#include <string.h>

#include "HAL_Log.h"
#include "HAL_Uart.h"
#include "HAL_Gpio.h"
#include "HAL_System.h"

#include "APP_Led.h"
#include "APP_Pluse.h"
#include "APP_Param.h"
#include "APP_Timer.h"
#include "APP_Detect.h"
#include "APP_Factory.h"

#include "lyy_start.h"

#include "USER_Config.h"


//短接标识
static uint8_t g_djLogo;


static void APP_prvRecvFunc(uint8_t *pucData, uint32_t ulLen)
{

    //短接
    if(strstr((char *)pucData,"write device_id:include") != NULL)  {

        g_djLogo = 1;
        APP_vLedSstSta(LED_GREEN,   LED_STA_QUICK_FLASH);
        APP_vLedSstSta(LED_RED,     LED_STA_QUICK_FLASH);
        return ;
    }  

#define TEST_GIFT_SEND_DATA      "BBBBBBBB"


    if(strstr((char *)pucData, TEST_GIFT_SEND_DATA) == NULL && 
                strstr((char *)pucData,"write device_id:include") == NULL)  {

        LYY_LOGD(">>>> TEST");
        HAL_vUartSend(HAL_UART_MAIN_BOARD, pucData, ulLen, 100);  
    }
         
 
#define WRITE_DEVICE_ID_CMD     "write device_id:"

    char cWarning[100]   = {0};
    char *pcDevid        = strchr((char *)pucData,':') + 1;

    if((strstr((char *)pucData, WRITE_DEVICE_ID_CMD) == NULL)) {

        sprintf(cWarning, " command is error, there is not \"%s\" ", WRITE_DEVICE_ID_CMD);

        LYY_LOGE("%s", cWarning);
        return ;    
    }

    if(strlen(pcDevid) > 32||strlen(pcDevid) < 4){

        sprintf(cWarning, "%s"," length is error,> 32 or < 4 ");

        LYY_LOGE("%s", cWarning);
        return ;        
    }

    for(uint8_t ucCnt = 0; ucCnt < strlen(pcDevid); ucCnt++) {

        if(pcDevid[ucCnt]>='0'&&pcDevid[ucCnt]<='9') //数字
            continue;

        if(pcDevid[ucCnt]>='a'&&pcDevid[ucCnt]<='z') //字母
            continue;

        if(pcDevid[ucCnt]>='A'&&pcDevid[ucCnt]<='Z') //字母
            continue;

        if(pcDevid[ucCnt]=='@'||pcDevid[ucCnt]=='-')
            continue;

        if(pcDevid[ucCnt]==':'||pcDevid[ucCnt]=='.')
            continue;

        if(pcDevid[ucCnt]=='_')
            continue;

        if(ucCnt == 0)
            sprintf(cWarning," there is invalid char in the deviceid,the invalid char[%d] is '%c', between ':' and '%c' ",ucCnt,pcDevid[ucCnt],pcDevid[ucCnt+1]);    
        else if(ucCnt == strlen(pcDevid) - 1)
            sprintf(cWarning," there is invalid char in the deviceid,the invalid char[%d] is '%c', between '%c' and 'end' ",ucCnt,pcDevid[ucCnt],pcDevid[ucCnt-1]);
        else
            sprintf(cWarning," there is invalid char in the deviceid,the invalid char[%d] is '%c', between '%c' and '%c' ",ucCnt,pcDevid[ucCnt],pcDevid[ucCnt-1],pcDevid[ucCnt+1]);

        LYY_LOGE("%s", cWarning);

        return ;
    }
    
    char pcDevId[32] = {0x00, };

    strcpy(pcDevId, strchr((char *)pucData,':') + 1);

    LYY_LOGD(">>> devid :[%s]", pcDevId);

    sprintf(cWarning, "\r\ndevice id is ok,devid:%s\r\n", pcDevId);

    HAL_vUartSend(HAL_UART_MAIN_BOARD, (uint8_t *)cWarning, strlen(cWarning), 100);

    APP_vDevInfoInit((const char *)pcDevId);

    HAL_vReset("> device ID_Config_OK < ", 200);
}



static void APP_prvScanFunc(uint8_t *pucData, uint32_t ulLen)
{

    (void)ulLen;

    //短接
    if(strstr((char *)pucData,"write device_id:include") != NULL)  {

        g_djLogo = 1;
        APP_vLedSstSta(LED_GREEN,   LED_STA_QUICK_FLASH);
        APP_vLedSstSta(LED_RED,     LED_STA_QUICK_FLASH);
    }  

}



static void APP_prvTestCoinIn(void *pvArg)
{

    APP_vPulseStart(APP_COIN_PLUSE, *((uint32_t *)pvArg));

    LYY_LOGD(">>> CoinIn test : [%d]", *((uint32_t *)pvArg));

}



static void APP_PrvUartSendTask(void *pvArg)
{

    (void)pvArg;

#define APP_TEST_UART_SEND_DATA             "write device_id:include(letter,number, @ - : . _)(4-32byte)"


    HAL_vUartSend(  HAL_UART_MAIN_BOARD,  
                    (uint8_t *)APP_TEST_UART_SEND_DATA, 
                    strlen(APP_TEST_UART_SEND_DATA), 
                    100);

    HAL_vUartSend(  HAL_UART_SCREEN,      
                    (uint8_t *)APP_TEST_UART_SEND_DATA, 
                    strlen(APP_TEST_UART_SEND_DATA), 
                    100);

    if(g_djLogo) {
        
        g_djLogo = 0x00;
        APP_vLedSstSta(LED_GREEN,   LED_STA_SLOW_FLASH);
        APP_vLedSstSta(LED_RED,     LED_STA_SLOW_FLASH);
    }

}



void APP_vFactoryInit(void)
{

    APP_vLedSstSta(LED_GREEN,   LED_STA_SLOW_FLASH);
    APP_vLedSstSta(LED_RED,     LED_STA_SLOW_FLASH);

    APP_vDetectInit();
    APP_Timer1msInit();

    APP_vDeteclRegisterFunc(APP_INPUT_COIN,     APP_prvTestCoinIn);


 #define SCAN_UART_BRT               (115200)

    HAL_vUartInit(HAL_UART_SCREEN,          SCAN_UART_BRT);
    HAL_vUartRecvProc(HAL_UART_SCREEN,      APP_prvScanFunc); 
    HAL_vUartRecvProc(HAL_UART_MAIN_BOARD,  APP_prvRecvFunc);

#define APP_UART_SEND_TASK           (4 * 1000)


    static struct HalSoftTimer AppUartSendTask;

    HAL_vSoftTimeInit(  &AppUartSendTask, 
                        APP_PrvUartSendTask, 
                        APP_UART_SEND_TASK,
                        NULL);

}
















