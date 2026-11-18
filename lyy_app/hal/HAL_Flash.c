/**
 * @file HAL_Flash.c
 * @brief BW20 VFS 参数文件及对端升级镜像缓存访问。
 * @note HAL_lFlashAddr 系列仅把兼容虚拟地址转换为缓存文件偏移，不直接操作物理 Flash。
 */
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
#include <limits.h>


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


/**
 * @brief 以 wb+ 重写参数或缓存文件。
 * @param[in] pcStore 相对于 VFS_REGION_1 的文件名。
 * @param[in] pucDuf 待写入字节。
 * @param[in] xSize 字节数，0 时仍打开并截断文件。
 * @return fwrite 实际写入字节数；打开失败返回 -1，调用者应比较期望长度。
 */
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


/**
 * @brief 获取 VFS 文件长度。
 * @param[in] pcStore 相对于 VFS_REGION_1 的文件名。
 * @return 文件长度（字节）。打开、定位或读取位置失败时返回负值。
 * @note Realtek VFS 的 fseek 成功时返回新偏移，失败时返回负值。
 */
long HAL_lFileSize(const char *pcStore)
{
    char path[HAL_FLASH_VFS_PATH_MAX_LEN] = {0};
    HAL_prvFileGetVfsPath(pcStore, path, sizeof(path));
    FILE *file = fopen(path, "rb");
    if (!file) return -1;

    long size = fseek(file, 0, SEEK_END) < 0 ? -1 : ftell(file);
    fclose(file);
    return size;
}

/**
 * @brief 在文件偏移处写入，文件不存在时尝试创建。
 * @param[in] pcStore VFS 文件名。
 * @param[in] pucDuf 源数据。
 * @param[in] xSize 请求写入字节数。
 * @param[in] xOffset 从文件头起的字节偏移。
 * @return 实际写入字节数。打开失败为 0，偏移越界、定位或关闭失败为 -1。
 * @note Realtek VFS 的 fseek 允许非零成功返回值。已有文件保留未覆盖部分，调用者须串行化同一文件的读写。
 */
int HAL_vFileWriteOffset(const char *pcStore, uint8_t *pucDuf, size_t xSize, size_t xOffset)
{
    if (xOffset > LONG_MAX) return -1;

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
    if (fseek((FILE *)pxFd, (long)xOffset, SEEK_SET) < 0) { fclose((FILE *)pxFd); return -1; }

    //lRet = fwrite(pucDuf, xSize, 1, (FILE *)pxFd);
    lRet = fwrite(pucDuf, 1, xSize, (FILE *)pxFd);

    LYY_LOGD("write : [%d]", lRet);

    if (fclose((FILE *)pxFd) < 0) return -1;
    return lRet;
}


/**
 * @brief 从指定偏移读取文件，可能因文件较短而只返回部分数据。
 * @param[in] pcStore VFS 文件名。
 * @param[out] pucDuf 至少 xSize 字节的目标缓冲。
 * @param[in] xSize 请求读取字节数。
 * @param[in] xOffset 字节偏移。
 * @return 实际读取字节数。打开失败为 0，偏移越界或定位失败为 -1。
 * @note Realtek VFS 的 fseek 成功时可能返回正偏移。
 */
int HAL_vFileReadOffset(const char *pcStore, uint8_t *pucDuf, size_t xSize, size_t xOffset)
{
    if (xOffset > LONG_MAX) return -1;

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
    if (fseek((FILE *)pxFd, (long)xOffset, SEEK_SET) < 0) { fclose((FILE *)pxFd); return -1; }

    //lRet = fread(pucDuf, xSize, 1, (FILE *)pxFd);
    lRet = fread(pucDuf, 1, xSize, (FILE *)pxFd);

    LYY_LOGD("read : [%d]", lRet);

    fclose((FILE *)pxFd);
    
    return lRet;


}


/**
 * @brief 校验镜像虚拟基址及容量后，将整个缓存文件截断为零长度。
 * @param[in] ulAddr 必须等于 FILE_NAME_HW_DEVICE_OTA_LOAD。
 * @param[in] xSize 仅用于容量校验，不代表局部擦除范围。
 * @retval 0 截断文件成功。
 * @retval -1 地址/长度非法或文件打开失败。
 */
int HAL_lFlashAddrErase(uint32_t ulAddr, size_t xSize)
{
    if (ulAddr != FILE_NAME_HW_DEVICE_OTA_LOAD || xSize > FILE_NAME_HW_DEVICE_OTA_SIZE) return -1;
    return HAL_vFileWrite(HAL_BOARD_OTA_FILE, (uint8_t *)"", 0) == 0 ? 0 : -1;
}


/**
 * @brief 将虚拟地址转换为缓存文件偏移，要求完整写入。
 * @param[in] ulAddr 镜像虚拟地址，基址起的偏移须落在容量范围内。
 * @param[in] pucDuf 源数据。
 * @param[in] xSize 请求字节数。
 * @retval 0 实际写入长度等于请求长度。
 * @retval -1 越界、定位失败或未完整写入。
 */
int HAL_lFlashAddrWrite(uint32_t ulAddr, uint8_t *pucDuf, size_t xSize)
{
    if (ulAddr < FILE_NAME_HW_DEVICE_OTA_LOAD || xSize > FILE_NAME_HW_DEVICE_OTA_SIZE ||
        ulAddr - FILE_NAME_HW_DEVICE_OTA_LOAD > FILE_NAME_HW_DEVICE_OTA_SIZE - xSize) return -1;
    return HAL_vFileWriteOffset(HAL_BOARD_OTA_FILE, pucDuf, xSize, ulAddr - FILE_NAME_HW_DEVICE_OTA_LOAD) == (int)xSize ? 0 : -1;
}



/**
 * @brief 从镜像缓存虚拟地址读取，拒绝越界和短读。
 * @param[in] ulAddr FILE_NAME_HW_DEVICE_OTA_LOAD 加镜像字节偏移。
 * @param[out] pucDuf 至少 xSize 字节的接收缓冲。
 * @param[in] xSize 要求完整读取的字节数。
 * @retval 0 完整读取成功。
 * @retval -1 地址越界、文件操作失败或缓存内容不足。
 */
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

