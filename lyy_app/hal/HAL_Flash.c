/*
 * @Author: linjunqi linjunqi@leyaoyao.org
 * @Date: 2023-10-11 17:26:03
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-05 20:56:14
 * @FilePath: \ESP8266_RTOS_SDK\lyy_app\main\hal\hal_flash.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#include "HAL_Log.h"
#include "HAL_Flash.h"

#include "HAL_Ota.h"
#include "vfs.h"
#include "ameba.h"

#define HAL_FLASH_VFS_PATH_MAX_LEN      (128U)
#define HAL_FLASH_VFS_PATH              "%s:%s"


static void HAL_prvFileGetVfsPath(const char *pcStore, char *pcPath, uint16_t ulPathLen)
{
    char *cPreFix;

    cPreFix = find_vfs_tag(VFS_REGION_1);

    DiagSnPrintf(   pcPath, 
                    ulPathLen, 
                    HAL_FLASH_VFS_PATH, 
                    cPreFix, 
                    pcStore);

}


int HAL_vFileWrite(const char *pcStore, uint8_t *pucDuf, size_t xSize)
{
    int  lRet = -1;
    char pcPath[HAL_FLASH_VFS_PATH_MAX_LEN] = {0x00, }; 

    HAL_prvFileGetVfsPath(  pcStore, 
                            pcPath, 
                            HAL_FLASH_VFS_PATH_MAX_LEN);

    vfs_file *pxFd = (vfs_file *)fopen(pcPath, "wb+");

    if (pxFd == NULL) {
        
        LYY_LOGE("Failed to open file for writing");

        return lRet;

    }

    //lRet = fwrite(pucDuf, xSize, 1, (FILE *)pxFd);
    lRet = fwrite(pucDuf, 1, xSize, (FILE *)pxFd);

    LYY_LOGD("write : [%d]", lRet);

    fclose((FILE *)pxFd);
    
    return lRet;

}


int HAL_vFileRead(const char *pcStore, uint8_t *pucDuf, size_t xSize)
{

    int  lRet                               = 0x00;
    char pcPath[HAL_FLASH_VFS_PATH_MAX_LEN] = {0x00, }; 

    HAL_prvFileGetVfsPath(  pcStore, 
                            pcPath, 
                            HAL_FLASH_VFS_PATH_MAX_LEN);

    vfs_file *pxFd = (vfs_file *)fopen(pcPath, "r");

    if (pxFd == NULL) {
        
        LYY_LOGE("Failed to open file for writing");

        return lRet;

    }

    //lRet = fread(pucDuf, xSize, 1, (FILE *)pxFd);
    lRet = fread(pucDuf, 1, xSize, (FILE *)pxFd);

    LYY_LOGD("read : [%d]", lRet);

    fclose((FILE *)pxFd);   

    return lRet;

}


void HAL_vFileClear(const char *pcStore)
{

    char pcPath[HAL_FLASH_VFS_PATH_MAX_LEN] = {0x00, }; 

    HAL_prvFileGetVfsPath(  pcStore, 
                            pcPath, 
                            HAL_FLASH_VFS_PATH_MAX_LEN);

    if(0x00 == remove(pcPath)) {

        LYY_LOGD("remove success");
    }else{

        LYY_LOGE("remove FAIL");
    }


}


int HAL_vFileWriteOffset(const char *pcStore, uint8_t *pucDuf, size_t xSize, size_t xOffset)
{

    int  lRet                               = 0x00;
    char pcPath[HAL_FLASH_VFS_PATH_MAX_LEN] = {0x00, }; 

    HAL_prvFileGetVfsPath(  pcStore, 
                            pcPath, 
                            HAL_FLASH_VFS_PATH_MAX_LEN);

    vfs_file *pxFd = (vfs_file *)fopen(pcPath, "rb+");
    if (pxFd == NULL)
    {
        pxFd = (vfs_file *)fopen(pcPath, "wb+"); // 文件不存在时创建
    }

    if (pxFd == NULL) return lRet;

    // 文件头开始进行偏移
    if (fseek((FILE *)pxFd, xOffset, SEEK_SET)) { fclose((FILE *)pxFd); return -1; }

    //lRet = fwrite(pucDuf, xSize, 1, (FILE *)pxFd);
    lRet = fwrite(pucDuf, 1, xSize, (FILE *)pxFd);

    LYY_LOGD("write : [%d]", lRet);

    fclose((FILE *)pxFd);
    
    return lRet;


}


int HAL_vFileReadOffset(const char *pcStore, uint8_t *pucDuf, size_t xSize, size_t xOffset)
{

    int  lRet                               = 0x00;
    char pcPath[HAL_FLASH_VFS_PATH_MAX_LEN] = {0x00, }; 

    HAL_prvFileGetVfsPath(  pcStore, 
                            pcPath, 
                            HAL_FLASH_VFS_PATH_MAX_LEN);

    vfs_file *pxFd = (vfs_file *)fopen(pcPath, "r");

    if (pxFd == NULL) {
        
        LYY_LOGE("Failed to open file for reading");

        return lRet;

    }

    // 文件头开始进行偏移
    if (fseek((FILE *)pxFd, xOffset, SEEK_SET)) { fclose((FILE *)pxFd); return -1; }

    //lRet = fread(pucDuf, xSize, 1, (FILE *)pxFd);
    lRet = fread(pucDuf, 1, xSize, (FILE *)pxFd);

    LYY_LOGD("read : [%d]", lRet);

    fclose((FILE *)pxFd);
    
    return lRet;


}


int HAL_lFlashAddrErase(uint32_t ulAddr, size_t xSize)
{
    if (ulAddr != FILE_NAME_HW_DEVICE_OTA_LOAD || xSize > FILE_NAME_HW_DEVICE_OTA_SIZE) return -1;
    return HAL_vFileWrite(HAL_BOARD_OTA_FILE, (uint8_t *)"", 0) == 0 ? 0 : -1;
}


int HAL_lFlashAddrWrite(uint32_t ulAddr, uint8_t *pucDuf, size_t xSize)
{
    if (ulAddr < FILE_NAME_HW_DEVICE_OTA_LOAD || xSize > FILE_NAME_HW_DEVICE_OTA_SIZE ||
        ulAddr - FILE_NAME_HW_DEVICE_OTA_LOAD > FILE_NAME_HW_DEVICE_OTA_SIZE - xSize) return -1;
    return HAL_vFileWriteOffset(HAL_BOARD_OTA_FILE, pucDuf, xSize, ulAddr - FILE_NAME_HW_DEVICE_OTA_LOAD) == (int)xSize ? 0 : -1;
}



int HAL_lFlashAddrRead(uint32_t ulAddr, uint8_t *pucDuf, size_t xSize)
{
    if (ulAddr < FILE_NAME_HW_DEVICE_OTA_LOAD || xSize > FILE_NAME_HW_DEVICE_OTA_SIZE ||
        ulAddr - FILE_NAME_HW_DEVICE_OTA_LOAD > FILE_NAME_HW_DEVICE_OTA_SIZE - xSize) return -1;
    return HAL_vFileReadOffset(HAL_BOARD_OTA_FILE, pucDuf, xSize, ulAddr - FILE_NAME_HW_DEVICE_OTA_LOAD) == (int)xSize ? 0 : -1;
}


void HAL_vFlashInit(void)
{

    LYY_LOGI("HAL_vFlashInit");

}

