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

    int  lRet                               = 0x00;
    char pcPath[HAL_FLASH_VFS_PATH_MAX_LEN] = {0x00, }; 

    HAL_prvFileGetVfsPath(  pcStore, 
                            pcPath, 
                            HAL_FLASH_VFS_PATH_MAX_LEN);

    vfs_file *pxFd = (vfs_file *)fopen(pcPath, "wb+");

    if (pxFd == NULL) {
        
        LYY_LOGE("Failed to open file for writing");

        return lRet;

    }

    lRet = fwrite(pucDuf, xSize, 1, (FILE *)pxFd);

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

    lRet = fread(pucDuf, xSize, 1, (FILE *)pxFd);

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

    vfs_file *pxFd = (vfs_file *)fopen(pcPath, "ab+");

    if (pxFd == NULL) {
        
        LYY_LOGE("Failed to open file for writing");

        return lRet;

    }

    // 文件头开始进行偏移
    fseek((FILE *)pxFd, xOffset, SEEK_SET);

    lRet = fwrite(pucDuf, xSize, 1, (FILE *)pxFd);

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
    fseek((FILE *)pxFd, xOffset, SEEK_SET);

    lRet = fread(pucDuf, xSize, 1, (FILE *)pxFd);

    LYY_LOGD("read : [%d]", lRet);

    fclose((FILE *)pxFd);
    
    return lRet;


}


int HAL_lFlashAddrErase(uint32_t ulAddr, size_t xSize)
{

    (void)ulAddr;
    (void)xSize;

    return 0x00;

}


int HAL_lFlashAddrWrite(uint32_t ulAddr, uint8_t *pucDuf, size_t xSize)
{

    (void)ulAddr;
    (void)pucDuf;
    (void)xSize;

    return 0x00;

}



int HAL_lFlashAddrRead(uint32_t ulAddr, uint8_t *pucDuf, size_t xSize)
{

    (void)ulAddr;
    (void)pucDuf;
    (void)xSize;

    return 0x00;

}


void HAL_vFlashInit(void)
{

    LYY_LOGI("HAL_vFlashInit");

}

