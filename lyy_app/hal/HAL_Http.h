#ifndef __HAL_HTTP_H__
#define __HAL_HTTP_H__


enum {

    HTTP_GET_SUCCESS,
    HTTP_GET_ERR,
};

typedef void (*vHttpDataCd)(char *pcData, uint32_t ulLen);
typedef void (*vHttpRes)(uint8_t ucRes, uint32_t ulLen);

extern void HAL_vHttpGet(const char *pcUrl, vHttpDataCd xHttpDataCd, vHttpRes xHttpRes);

#endif


