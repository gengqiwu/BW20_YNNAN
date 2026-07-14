/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2024-12-08 14:17:54
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-08 14:26:36
 * @FilePath: \ameba-rtos\lyy_app\app\APP_Upgrade.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "HAL_Log.h"
#include "HAL_Ota.h"
#include "HAL_Http.h"


#include "APP_NetWork.h"
#include "APP_Upgrade.h"
#include "thing_mqtt.h"

#include "lyy_device.h"



void APP_UpgradeDelNetWorkDev(  uint8_t ucUpgradeType, 
                                uint8_t ucDataType, 
                                uint8_t *pucData, 
                                uint32_t ulLen, 
                                vUpdataDataCd vDataFunc, 
                                vUpdataRes vRes)
{

    if(!pucData || !ulLen)  {
        
        return ;
    }
    
    (void)ulLen;

    switch(ucUpgradeType)
    {
        case APP_UPGRADE_HTTP : {

            if(APP_UPGRADE_APP == ucDataType) {

                HAL_vOtaHttp((const char *)pucData, NULL);

            }else if (APP_UPGRADE_DEVICE == ucDataType) {

                HAL_vHttpGet((const char *)pucData, vDataFunc, vRes);
            }
            else if (APP_UPGRADE_SCREEN == ucDataType)
            {
                HAL_vHttpGet((const char *)pucData, vDataFunc, vRes);
            }
        }break;

        case APP_UPGRADE_TCP : {

        }break;
    }


}



void APP_UpgradeDel(uint8_t *pucData, uint32_t ulLen)
{

    if(!pucData || !ulLen)  {

        return ;
    }

    APP_UPGRADE_INFO_T *pxUpgradeInfo = (APP_UPGRADE_INFO_T *)pucData;

    switch(pxUpgradeInfo->ucDevType)
    {

        case APP_UPGRADE_NETWORK_DEV : {

        }break;

        case APP_UPGRADE_BOARD : {
            
            APP_UpgradeDelNetWorkDev(   pxUpgradeInfo->ucUpgradeType, 
                                        pxUpgradeInfo->ucDataType, 
                                        pxUpgradeInfo->pucData, 
                                        pxUpgradeInfo->usDataLen,
                                        pxUpgradeInfo->vDataFunc,
                                        pxUpgradeInfo->vRes);
        }break;

    }


}




void APP_UpgradeInit(void)
{


}



