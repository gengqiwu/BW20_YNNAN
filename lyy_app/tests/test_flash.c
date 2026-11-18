#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "HAL_Flash.h"
#include "HAL_Ota.h"
int main(void)
{
    uint8_t data[1024], readback[1024];
    for(unsigned i=0;i<sizeof(data);++i) data[i]=(uint8_t)i;
    assert(HAL_lFlashAddrErase(FILE_NAME_HW_DEVICE_OTA_LOAD, FILE_NAME_HW_DEVICE_OTA_SIZE)==0);
    assert(HAL_lFlashAddrWrite(FILE_NAME_HW_DEVICE_OTA_LOAD,data,512)==0);
    assert(HAL_lFlashAddrWrite(FILE_NAME_HW_DEVICE_OTA_LOAD+512,data+512,512)==0);
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD,readback,sizeof(readback))==0);
    assert(!memcmp(data,readback,sizeof(data)));
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD-1,readback,1)<0);
    assert(HAL_lFlashAddrWrite(FILE_NAME_HW_DEVICE_OTA_LOAD+FILE_NAME_HW_DEVICE_OTA_SIZE-1,data,2)<0);
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD,readback,FILE_NAME_HW_DEVICE_OTA_SIZE+1)<0);
    assert(HAL_lFlashAddrErase(FILE_NAME_HW_DEVICE_OTA_LOAD+1,FILE_NAME_HW_DEVICE_OTA_SIZE)<0);
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD+1024,readback,1)<0);
    assert(HAL_lFlashAddrErase(FILE_NAME_HW_DEVICE_OTA_LOAD,FILE_NAME_HW_DEVICE_OTA_SIZE)==0);
    assert(HAL_lFlashAddrRead(FILE_NAME_HW_DEVICE_OTA_LOAD,readback,1)<0);
    puts("flash tests passed: real file-backed OTA cache, offset IO, readback, truncation, bounds");
    return 0;
}
