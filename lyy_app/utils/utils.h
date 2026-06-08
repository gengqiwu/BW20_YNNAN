#ifndef __UTILS_H__
#define __UTILS_H__


extern int utils_lHexToStringLower(char* pDst, const unsigned char* pSrc, int nSrcLength);
extern int utils_lhexToString(char* pDst, const unsigned char* pSrc, int nSrcLength);


extern int utils_lAesPkcs7Padding(uint8_t *psrc, uint8_t *pobj, uint16_t len);
extern int utils_lAesECB128Encrypt( uint8_t *psrc, uint8_t *pobj, uint16_t len, uint8_t *key);
extern int utils_lAesECB128Decrypt( uint8_t *psrc, uint8_t *pobj, uint16_t len, uint8_t *key);

extern float    lyy_bytesTofloat2(uint16_t input);
extern uint16_t lyy_float2Tobytes(float input);

extern int      String2ToBytes(const char* pSrc, char* pDst, int nSrcLength);
extern uint8_t  check_xor_memory(const uint8_t *pTemp, uint16_t len);
extern uint16_t data_arrayTolittle16(uint8_t *little);
extern void     data_little16Toarray(uint16_t vaule, uint8_t *pout);

extern uint16_t utils_BSWAP16(uint16_t data);
extern uint16_t utils_CRC16Modbus(uint8_t *pu8Data, uint16_t u16Len);

#endif


