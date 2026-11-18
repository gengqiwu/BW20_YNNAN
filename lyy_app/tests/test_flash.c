/**
 * @file test_flash.c
 * @brief 真实文件缓存适配的偏移读写、截断、短读及虚拟地址越界回归。
 * @note TEST_FLASH_PREFIX 指向工程外临时目录，避免改写工程文件。
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "HAL_Flash.h"
#include "HAL_Ota.h"

/** @brief GNU ld 提供的未包装主机 fseek，用于实际文件定位。 */
int __real_fseek(FILE *stream, long offset, int origin);

/**
 * @brief 在主机复现 Realtek VFS 的文件定位返回语义。
 * @param[in,out] stream 测试文件句柄。
 * @param[in] offset 相对于 origin 的字节偏移。
 * @param[in] origin 定位基准。
 * @return 成功时返回新偏移，失败时返回负值。
 */
int __wrap_fseek(FILE *stream, long offset, int origin)
{
    if (__real_fseek(stream, offset, origin) != 0) return -1;
    return (int)ftell(stream);
}

/** @brief 验证图片及 OTA 缓存的非零偏移读写。 @return 全部断言通过时返回 0。 */
int main(void)
{
    uint8_t data[1024], readback[1024];
    for(unsigned i=0;i<sizeof(data);++i) data[i]=(uint8_t)i;
    assert(HAL_lFlashAddrErase(FILE_NAME_HW_DEVICE_OTA_LOAD, FILE_NAME_HW_DEVICE_OTA_SIZE)==0);
    assert(HAL_lFlashAddrWrite(FILE_NAME_HW_DEVICE_OTA_LOAD,data,512)==0);
    assert(HAL_lFlashAddrWrite(FILE_NAME_HW_DEVICE_OTA_LOAD+512,data+512,512)==0);
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD,readback,sizeof(readback))==0);
    assert(!memcmp(data,readback,sizeof(data)));
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD+512,readback,512)==0);
    assert(!memcmp(data+512,readback,512));
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD-1,readback,1)<0);
    assert(HAL_lFlashAddrWrite(FILE_NAME_HW_DEVICE_OTA_LOAD+FILE_NAME_HW_DEVICE_OTA_SIZE-1,data,2)<0);
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD,readback,FILE_NAME_HW_DEVICE_OTA_SIZE+1)<0);
    assert(HAL_lFlashAddrErase(FILE_NAME_HW_DEVICE_OTA_LOAD+1,FILE_NAME_HW_DEVICE_OTA_SIZE)<0);
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD+1024,readback,1)<0);
    assert(HAL_lFlashAddrErase(FILE_NAME_HW_DEVICE_OTA_LOAD,FILE_NAME_HW_DEVICE_OTA_SIZE)==0);
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD,readback,1)<0);
    assert(HAL_vFileWrite("picture", data, sizeof(data)) == sizeof(data));
    assert(HAL_lFileSize("picture") == sizeof(data));
    assert(HAL_lFileSize("missing-picture") < 0);
    assert(HAL_vFileReadOffset("picture", readback, 1000, 0) == 1000);
    assert(HAL_vFileReadOffset("picture", readback, 24, 1000) == 24);
    assert(!memcmp(data + 1000, readback, 24));
    puts("flash tests passed: VFS position-returning seek, nonzero image and OTA offsets, size and bounds");
    return 0;
}
