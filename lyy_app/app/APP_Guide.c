#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "FreeRTOS.h"
#include "task.h"
#include "HAL_Log.h"
#include "HAL_Http.h"
#include "HAL_System.h"
#include "APP_Param.h"
#include "APP_Guide.h"
#include "utils.h"
#include "cJSON.h"

#define APP_HTTP_RECV_DATA_LEN_MAX (1024U + 512U)
#define APP_GUIDE_DECRYPT_LEN_MAX  (512U)

typedef enum {
    GUIDE_IDLE,
    GUIDE_RUNNING,
    GUIDE_END,
    GUIDE_FAIL
} APP_GUIDE_STA_T;

typedef struct {
    char *pHttpRevData;
    uint32_t ulDataLen;
    APP_GUIDE_STA_T xGuideSta;
} APP_GUIDE_T;

static APP_GUIDE_T g_xAppGuideInfo;

static void APP_prvHttpDataParse(char *pcData, uint32_t ulLen)
{
    cJSON *pxRoot = NULL;
    cJSON *pxDecrypt = NULL;
    cJSON *pxItem;
    uint8_t *pucDecrypt = NULL;
    APP_SER_T xAppSer = {0};
    int lDecryptLen;
    size_t xHexLen;

    if (!pcData || !ulLen) {
        return;
    }
    pxRoot = cJSON_Parse(pcData);
    if (!pxRoot) {
        goto exit;
    }
    pxItem = cJSON_GetObjectItem(pxRoot, "data");
    if (!pxItem || pxItem->type != cJSON_String || !pxItem->valuestring) {
        goto exit;
    }
    xHexLen = strlen(pxItem->valuestring);
    if (!xHexLen || xHexLen > APP_GUIDE_DECRYPT_LEN_MAX * 2 || xHexLen % 32) {
        goto exit;
    }
    for (size_t i = 0; i < xHexLen; ++i) {
        if (!isxdigit((unsigned char)pxItem->valuestring[i])) {
            goto exit;
        }
    }
    pucDecrypt = pvPortMalloc(APP_GUIDE_DECRYPT_LEN_MAX + 1);
    if (!pucDecrypt) {
        goto exit;
    }
    lDecryptLen = String2ToBytes(pxItem->valuestring, (char *)pucDecrypt, xHexLen);
    lDecryptLen = utils_lAesECB128Decrypt(pucDecrypt, pucDecrypt, lDecryptLen,
                                        (uint8_t *)APP_pcGetProductSecret());
    if (lDecryptLen <= 0 || lDecryptLen > (int)APP_GUIDE_DECRYPT_LEN_MAX) {
        goto exit;
    }
    pucDecrypt[lDecryptLen] = '\0';
    pxDecrypt = cJSON_Parse((char *)pucDecrypt);
    if (!pxDecrypt) {
        goto exit;
    }

    /* Missing guide fields retain the current settings, including device type. */
    snprintf(xAppSer.pcProductKey, sizeof(xAppSer.pcProductKey), "%s", APP_pcGetProductKey());
    snprintf(xAppSer.pcProductSecret, sizeof(xAppSer.pcProductSecret), "%s", APP_pcGetProductSecret());
    snprintf(xAppSer.pcSerUrl, sizeof(xAppSer.pcSerUrl), "%s", APP_pcGetSerUrl());
    snprintf(xAppSer.pcDevType, sizeof(xAppSer.pcDevType), "%s", APP_pcGetDevType());
    xAppSer.ulPort = APP_usGetSerPort();

    pxItem = cJSON_GetObjectItem(pxDecrypt, "broker");
    if (pxItem && pxItem->type == cJSON_String && pxItem->valuestring) {
        const char *pcHost = pxItem->valuestring;
        const char *pcPort;
        char *pcEnd;
        long lPort;
        if (strncmp(pcHost, "mqtts://", strlen("mqtts://"))) {
            goto exit;
        }
        pcHost += strlen("mqtts://");
        pcPort = strchr(pcHost, ':');
        if (!pcPort || pcPort == pcHost || (size_t)(pcPort - pcHost) >= sizeof(xAppSer.pcSerUrl)) {
            goto exit;
        }
        lPort = strtol(pcPort + 1, &pcEnd, 10);
        if (pcEnd == pcPort + 1 || (*pcEnd && strcmp(pcEnd, "/")) || lPort <= 0 || lPort > 65535) {
            goto exit;
        }
        memcpy(xAppSer.pcSerUrl, pcHost, pcPort - pcHost);
        xAppSer.pcSerUrl[pcPort - pcHost] = '\0';
        xAppSer.ulPort = (uint16_t)lPort;
    }

    pxItem = cJSON_GetObjectItem(pxDecrypt, "productKey");
    if (pxItem && pxItem->type == cJSON_String && pxItem->valuestring && pxItem->valuestring[0]) {
        if (strlen(pxItem->valuestring) >= sizeof(xAppSer.pcProductKey)) {
            goto exit;
        }
        strcpy(xAppSer.pcProductKey, pxItem->valuestring);
    }
    pxItem = cJSON_GetObjectItem(pxDecrypt, "productSecret");
    if (pxItem && pxItem->type == cJSON_String && pxItem->valuestring && pxItem->valuestring[0]) {
        if (strlen(pxItem->valuestring) >= sizeof(xAppSer.pcProductSecret)) {
            goto exit;
        }
        strcpy(xAppSer.pcProductSecret, pxItem->valuestring);
    }

    if (strcmp(xAppSer.pcProductKey, APP_pcGetProductKey()) ||
        strcmp(xAppSer.pcProductSecret, APP_pcGetProductSecret()) ||
        strcmp(xAppSer.pcSerUrl, APP_pcGetSerUrl()) ||
        xAppSer.ulPort != APP_usGetSerPort()) {
        APP_lSerSetInfo(&xAppSer);
        HAL_vReset("guide update", 1000);
    }
    g_xAppGuideInfo.xGuideSta = GUIDE_END;

exit:
    if (pucDecrypt) {
        vPortFree(pucDecrypt);
    }
    cJSON_Delete(pxDecrypt);
    cJSON_Delete(pxRoot);
}

/* BW HTTP delivers body chunks, unlike the ESP whole-body callback. */
static void APP_prvHttpDataRvCd(char *pcData, uint32_t ulLen)
{
    if (!g_xAppGuideInfo.pHttpRevData || g_xAppGuideInfo.xGuideSta != GUIDE_RUNNING) {
        return;
    }
    if (ulLen >= APP_HTTP_RECV_DATA_LEN_MAX - g_xAppGuideInfo.ulDataLen) {
        g_xAppGuideInfo.xGuideSta = GUIDE_FAIL;
        return;
    }
    memcpy(g_xAppGuideInfo.pHttpRevData + g_xAppGuideInfo.ulDataLen, pcData, ulLen);
    g_xAppGuideInfo.ulDataLen += ulLen;
    g_xAppGuideInfo.pHttpRevData[g_xAppGuideInfo.ulDataLen] = '\0';
}

static void APP_prvHttpRes(uint8_t ucRes, uint32_t ulLen)
{
    (void)ulLen;
    if (ucRes == HTTP_GET_SUCCESS && g_xAppGuideInfo.xGuideSta == GUIDE_RUNNING) {
        APP_prvHttpDataParse(g_xAppGuideInfo.pHttpRevData, g_xAppGuideInfo.ulDataLen);
    }
    if (g_xAppGuideInfo.xGuideSta != GUIDE_END) {
        g_xAppGuideInfo.xGuideSta = GUIDE_FAIL;
        LYY_LOGE("### guide request failed");
    }
    if (g_xAppGuideInfo.pHttpRevData) {
        vPortFree(g_xAppGuideInfo.pHttpRevData);
        g_xAppGuideInfo.pHttpRevData = NULL;
    }
}

void APP_vGuideLoop(void)
{
    uint8_t pucDeviceIdBuff[48] = {0};
    char pcDeviceIdStr[sizeof(pucDeviceIdBuff) * 2 + 1] = {0};
    char pcGuideUrl[256] = {0};
    int lLen;

    if (g_xAppGuideInfo.xGuideSta != GUIDE_IDLE) {
        return;
    }
    g_xAppGuideInfo.pHttpRevData = pvPortMalloc(APP_HTTP_RECV_DATA_LEN_MAX);
    if (!g_xAppGuideInfo.pHttpRevData) {
        return;
    }
    memset(g_xAppGuideInfo.pHttpRevData, 0, APP_HTTP_RECV_DATA_LEN_MAX);
    g_xAppGuideInfo.ulDataLen = 0;
    lLen = utils_lAesECB128Encrypt((uint8_t *)APP_pcGetParamDevId(), pucDeviceIdBuff,
        strlen(APP_pcGetParamDevId()), (uint8_t *)APP_pcGetProductSecret());
    utils_lHexToStringLower(pcDeviceIdStr, pucDeviceIdBuff, lLen);
    snprintf(pcGuideUrl, sizeof(pcGuideUrl),
        "https://sgiot.starthing.com/openapi/v1/dev/%s/device?deviceId=%s",
        APP_pcGetProductKey(), pcDeviceIdStr);
    g_xAppGuideInfo.xGuideSta = GUIDE_RUNNING;
    HAL_vHttpGet(pcGuideUrl, APP_prvHttpDataRvCd, APP_prvHttpRes);
}
