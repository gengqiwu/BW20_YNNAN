#ifndef __HAL_FLASH_H__
#define __HAL_FLASH_H__


extern void HAL_vFlashInit(void);

extern void HAL_vFileClear(const char *pcStore);
extern int  HAL_vFileRead(const char *pcStore,  uint8_t *pucDuf, size_t xSize);
extern int  HAL_vFileWrite(const char *pcStore, uint8_t *pucDuf, size_t xSize);
extern int  HAL_vFileReadOffset(const char *pcStore, uint8_t *pucDuf, size_t xSize, size_t xOffset);
extern int  HAL_vFileWriteOffset(const char *pcStore, uint8_t *pucDuf, size_t xSize, size_t xOffset);


extern int HAL_lFlashAddrErase(uint32_t ulAddr, size_t xSize);
extern int HAL_lFlashAddrRead(uint32_t ulAddr, uint8_t *pucDuf, size_t xSize);
extern int HAL_lFlashAddrWrite(uint32_t ulAddr, uint8_t *pucDuf, size_t xSize);


#endif

