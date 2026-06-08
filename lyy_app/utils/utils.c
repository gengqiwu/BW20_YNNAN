#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bsp_aes.h"

#define UTILS_AES_BLOCK                   (16U)


/* 填充方式 */
int utils_lAesPkcs7Padding(uint8_t *psrc, uint8_t *pobj, uint16_t len)
{
    /* 需要填充的字节长度 */
    unsigned char value = (UTILS_AES_BLOCK - (len % UTILS_AES_BLOCK));
    /* 把原数据copy一下 */
    memcpy(pobj,psrc,len);
    /* 填充的字节为填充的长度 */
    memset(pobj+len,value,value);

    return len+value;
}


/* aes加密（ECB，128位） */
int utils_lAesECB128Encrypt( uint8_t *psrc, uint8_t *pobj, uint16_t len, uint8_t *key)
{
    unsigned short i = 0;
    unsigned short length;
        
    length = utils_lAesPkcs7Padding(psrc, pobj, len);
        /* 以十六个字节为一块进行加密（计算加密的明文块) */
    length = ( length / UTILS_AES_BLOCK);
    
    for( ; i < length ; i++) 
    {
        AES128_ECB_encrypt(pobj+(UTILS_AES_BLOCK*i), key, pobj+(UTILS_AES_BLOCK*i));
    }
        
    return ( length * UTILS_AES_BLOCK);
}


/* aes128 ecb 解密方式*/
int utils_lAesECB128Decrypt( uint8_t *psrc, uint8_t *pobj, uint16_t len, uint8_t *key)
{
    /* 数据大小不为十六的倍速，视为密文丢失，不再解析 */
    if((len % UTILS_AES_BLOCK) != 0x00) 
    {
        return 0x00;
    }
        
    unsigned short i = 0;
    unsigned char UTILS_AES_BLOCKSix = (len / UTILS_AES_BLOCK);
    
    for( ; i < UTILS_AES_BLOCKSix ; i++) 
    {
        AES128_ECB_decrypt(psrc + (UTILS_AES_BLOCK*i), key, pobj + (UTILS_AES_BLOCK*i));
    }

    return len;    
}


int utils_lHexToStringLower(char* pDst, const unsigned char* pSrc, int nSrcLength)
{
	const char hexTab[]="0123456789abcdef";	// 0x0-0xf的字符查找表，小写
	int i;

	for (i = 0; i < nSrcLength; i++)
    {
		*pDst++ = hexTab[*pSrc >> 4];		// 输出高4位
		*pDst++ = hexTab[*pSrc & 0x0f];		// 输出低4位
		pSrc++;
	}

	// 输出字符串加个结束符
	*pDst = '\0';

	// 返回目标字符串长度
	return (nSrcLength * 2);

}


int utils_lhexToString(char* pDst, const unsigned char* pSrc, int nSrcLength)
{
	const char hexTab[]="0123456789ABCDEF";	// 0x0-0xf的字符查找表
	int i;

	for (i = 0; i < nSrcLength; i++)
    {
		*pDst++ = hexTab[*pSrc >> 4];		// 输出高4位
		*pDst++ = hexTab[*pSrc & 0x0f];		// 输出低4位
		pSrc++;
	}

	// 输出字符串加个结束符
	*pDst = '\0';

	// 返回目标字符串长度
	return (nSrcLength * 2);
}



/**
 * @description: float2转2字节数据
 * @param {*}
 * @return {*}
*/
uint16_t lyy_float2Tobytes(float input)
{
    uint16_t output = 0;
    output = input*10;
    return output;
}



/**
 * @description: 2字节数据转float2 除以10
 * @param {*}
 * @return {*}
*/
float lyy_bytesTofloat2(uint16_t input)
{
    float output = 0.0;
    output = input/10;
    return output;
}


int String2ToBytes(const char* pSrc, char* pDst, int nSrcLength)
{
	int i;
	
	for (i = 0; i < nSrcLength; i += 2)
	{
	// 输出高4位
		if ((*pSrc >= '0') && (*pSrc <= '9'))
		{
			*pDst = (*pSrc - '0') << 4;
		}
        else if((*pSrc >= 'A') && (*pSrc <= 'F'))
		{
			*pDst = (*pSrc - 'A' + 10) << 4;
		}
        else if((*pSrc >= 'a') && (*pSrc <= 'f'))//lyz 20240118 小写支持
        {
            *pDst = (*pSrc - 'a' + 10) << 4;
        }

		pSrc++;

		// 输出低4位
		if ((*pSrc>='0') && (*pSrc<='9'))
		{
			*pDst |= *pSrc - '0';
		}
        else if((*pSrc >= 'A') && (*pSrc <= 'F'))
		{
			*pDst |= *pSrc - 'A' + 10;
		}
        else if((*pSrc >= 'a') && (*pSrc <= 'f'))//lyz 20240118 小写支持
        {
            *pDst |= *pSrc - 'a' + 10;
        }


		pSrc++;
		pDst++; 
	}
	
	// 返回目标数据长度
	return (nSrcLength / 2);
}


/**
 * @brief 
 * check_xor_memory
 * @param pTemp 
 * @param len 
 * @return uint8_t 
 */
uint8_t check_xor_memory(const uint8_t *pTemp, uint16_t len)
{
    uint8_t chksum = 0x00;
    uint16_t index  = 0x00;
    for(index = 0x00;index <len; index++)
	  {
        chksum ^= *pTemp;
        pTemp++;
    }

    return chksum;
}


/**
 * @brief 
 * 数组转小端word型
 * @param little 
 * @return lyy_uint16_t 
 */
uint16_t data_arrayTolittle16(uint8_t *little)
{
	uint16_t len = little[1];
    return little[0]+(len<<8);
}

/**
 * @brief 
 * 小端word型转数组
 * @param vaule 
 * @param pout 
 */
void data_little16Toarray(uint16_t vaule, uint8_t *pout)
{
    pout[0] = vaule&0xff;
    pout[1] = vaule>>8;
}


uint16_t utils_BSWAP16(uint16_t data)
{
    return (uint16_t)((((uint16_t)(data) & 0x00ff) << 8) | \
                      (((uint16_t)(data) & 0xff00) >> 8));
}


uint16_t utils_CRC16Modbus(uint8_t *pu8Data, uint16_t u16Len)
{
	uint16_t Crc16 = 0xFFFF;
	uint16_t i, j;
	
	for( j = 0; j < u16Len; j++){
		Crc16 = Crc16^pu8Data[j];
		for( i = 0; i < 8; i++){
        if((Crc16&0x0001) > 0){
          Crc16 = Crc16>>1;
          Crc16 = Crc16^0xA001;
        }else{
            Crc16 = Crc16>>1;
        }
		}
	}
	return Crc16;
}

