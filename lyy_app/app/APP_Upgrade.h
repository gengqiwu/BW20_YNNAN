#ifndef __APP_UPGRADE_H__
#define __APP_UPGRADE_H__

#include <stdint.h>

enum 
{
    APP_UPGRADE_NETWORK_DEV,            /* 联网设备 */
    APP_UPGRADE_BOARD,                  /* 主板 */
};


enum 
{
    APP_UPGRADE_APP,
    APP_UPGRADE_SDK,
    APP_UPGRADE_DEVICE,
};


enum 
{

    APP_UPGRADE_TCP,
    APP_UPGRADE_HTTP,
};


typedef void (*vUpdataDataCd)(char *pcData, uint32_t ulLen);
typedef void (*vUpdataRes)(uint8_t ucRes, uint32_t ulLen);

typedef struct 
{

    uint8_t         ucDevType;
    uint8_t         ucUpgradeType;  
    uint8_t         ucDataType;     
    uint16_t        usDataLen;
    uint8_t         *pucData;
    vUpdataDataCd   vDataFunc;
    vUpdataRes      vRes;
}APP_UPGRADE_INFO_T;



extern void APP_UpgradeInit(void);
extern void APP_UpgradeDel(uint8_t *pucData, uint32_t ulLen);
extern void APP_UpgradeDelNetWorkDev(   uint8_t ucUpgradeType, 
                                        uint8_t ucDataType, 
                                        uint8_t *pucData, 
                                        uint32_t ulLen, 
                                        vUpdataDataCd vDataFunc, 
                                        vUpdataRes vRes);

#endif



