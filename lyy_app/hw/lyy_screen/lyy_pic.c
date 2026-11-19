/**
 * @file lyy_pic.c
 * @brief 支付渠道图片 URL 维护、HTTP 下载及经主板 D6 透传的屏幕分包传输。
 */
/*
 * @author: lyz
 * @Date: 2022-10-18 09:58:29
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-09 15:38:17
 * @Description: 
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>


#include "utils.h"
#include "cJSON.h"

#include "APP_NetWork.h"
#include "APP_Param.h"

#include "HAL_Log.h"
#include "HAL_Wifi.h"
#include "HAL_Http.h"
#include "HAL_Uart.h"
#include "HAL_Flash.h"
#include "HAL_System.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "thing_iot.h"
#include "thing_mqtt.h"
#include "thing_event.h"
#include "thing_property.h"


#include "lyy_tjc.h"
#include "lyy_pic.h"
#include "lyy_para.h"
#include "lyy_start.h"
#include "lyy_zcj.h"
#include "thing_mqtt.h"

#include "USER_Config.h"

#if LYY_SEND_PIC_PACKET_SIZE + LYY_SEND_PIC_HEADER_SIZE > ZCJ_SCREEN_PAYLOAD_MAX
#error Screen picture packet exceeds the D6 screen payload capacity
#endif



INFOR_PIC_VAIR picVair;


char lyy_screenType[64] = LYY_SCREEN_TYPC_00;
char lyy_screen[64] = LYY_SCREEN_DEVICE_4880x243;


LYY_SENDPIC sendPic;
/** @brief 已在末包发出后收到屏幕的 FD FF FF FF 完成确认。 */
static uint8_t picTransferComplete;
static uint8_t g_SemSta;


static uint32_t g_ulWriteOffset;
static char g_pcPath[64];

/**
 * @brief 生成钱包图标或扫码提示图的 Flash 缓存文件名。
 * @param[out] path 文件名缓冲区。
 * @param[in] size 缓冲区容量，包含结尾的 NUL。
 * @param[in] wallet 从 0 开始的支付渠道序号。
 * @param[in] type PIG_WALLET 或 PIG_SCAN。
 */
static void picture_cache_path(char *path, size_t size, unsigned int wallet, uint8_t type)
{
    snprintf(path, size, type == PIG_WALLET ? "PAY_WALLET_%u" : "PAY_SCAN_%u", wallet);
}

SemaphoreHandle_t hw_httpGetPicUrlSem  = NULL;
SemaphoreHandle_t hw_httpTranPicUrlSem = NULL;


static INFOR_PIC_Flash_INFO g_pxTjcPicFlashInfo;

void (*tjc_pic_result_cb)(INFOR_PIC_VAIR *) = NULL;

/**
 * @brief 通过统一屏幕发送入口转交图片控制命令或分包数据。
 * @param[in] data 待发送字节缓冲区。
 * @param[in] len 字节数，传给 uint16_t 长度入口，调用方须控制不超过其范围。
 * @note 发送走主板 D6 屏幕透传，不再直接写屏幕串口；不返回发送结果。
 */
void lyy_display_send(unsigned char *data, unsigned int len)
{
    lyy_tjc_cmd_send((char *)data, len);
}


int is_empty(const char *str) 
{
    return str == NULL || str[0] == '\0';
}

/**
 * @brief 装载渠道图片元数据并启动 URL 获取流程。
 * @param[in] pic_result_cb 图片处理完成回调，须非空。
 * @retval -1 回调为空或元数据分配失败。
 * @retval -2 支付渠道数为零。
 * @retval 0 初始化流程结束。
 * @note 渠道数截断至 CHANNELINFO_MAX；为全部渠道的 PIC_INFO_MAX 张图片分配元数据空间。
 */
int lyy_hw_tjc_pic_info_start(void (*pic_result_cb)(INFOR_PIC_VAIR *))
{

    uint8_t ucUpdateLogo = 0x00;

    PAY_PARAM_DEF *pay = get_pay_param();

    if(pic_result_cb == NULL)  
        return -1;

    if(pay->chnl_sum == 0)    
        return -2;


    tjc_pic_result_cb = pic_result_cb;

    if (pay->chnl_sum > CHANNELINFO_MAX) pay->chnl_sum = CHANNELINFO_MAX;
    HAL_vFileRead(  FILE_NAME_HW_PARA_TJC_PIC_INFO, 
                    &g_pxTjcPicFlashInfo.ucWalletNumSum, 
                    sizeof(g_pxTjcPicFlashInfo.ucWalletNumSum));


    LYY_LOGD("walletNum_sum = [%d]", g_pxTjcPicFlashInfo.ucWalletNumSum);

    // 已经存在了钱包、清除钱包信息
    if(((g_pxTjcPicFlashInfo.ucWalletNumSum != pay->chnl_sum) && (pay->chnl_sum  > 0)) ) {


        lyy_pic_infoAllClean();

        LYY_LOGD("================== pay->chnl_sum update : [%d] ", pay->chnl_sum);

        if(pay->chnl_sum > CHANNELINFO_MAX){

            g_pxTjcPicFlashInfo.ucWalletNumSum = CHANNELINFO_MAX;
        }else {

            g_pxTjcPicFlashInfo.ucWalletNumSum = pay->chnl_sum;
        }

        HAL_vFileWrite( FILE_NAME_HW_PARA_TJC_PIC_INFO, 
                        &g_pxTjcPicFlashInfo.ucWalletNumSum, 
                        sizeof(g_pxTjcPicFlashInfo.ucWalletNumSum));

        ucUpdateLogo = 1;

    }


    if(!g_pxTjcPicFlashInfo.pucData) { 

        g_pxTjcPicFlashInfo.pucData = malloc(CHANNELINFO_MAX * PIC_INFO_MAX * sizeof(INFOR_PIC_INFO));
    }

    if(!g_pxTjcPicFlashInfo.pucData) {

        tjc_setProcess(TJC_PIC_NULL);
        LYY_LOGE("### malloc err");
        return -1;
    }

    memset(g_pxTjcPicFlashInfo.pucData, 
            0x00, 
            g_pxTjcPicFlashInfo.ucWalletNumSum * 2 * sizeof(INFOR_PIC_INFO));

    /* 不需要更新数据从文件读数据 */
    if(!ucUpdateLogo) {

        HAL_vFileRead(  FILE_NAME_HW_PARA_TJC_PIC_DATA_INFO, 
                        g_pxTjcPicFlashInfo.pucData, 
                        (g_pxTjcPicFlashInfo.ucWalletNumSum * 2 * sizeof(INFOR_PIC_INFO)));

    }

    if(g_pxTjcPicFlashInfo.ucWalletNumSum > 0) {

        tjc_setProcess(TJC_PIC_GET_URL);
        LYY_LOGD("walletNum_sum = [%d]", g_pxTjcPicFlashInfo.ucWalletNumSum);
    }

	return 0;

}
/**
 * @brief 
 * 清除所有图片信息
 *
 */
void lyy_pic_infoAllClean(void)
{
    LYY_LOGD("==================  delete start");

    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_INFO);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_DATA_INFO);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PACKET_TITLE);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_TITLE_0);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_TITLE_1);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_TITLE_2);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_TITLE_3);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_WALLET_0);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_WALLET_1);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_WALLET_2);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_WALLET_3);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_SCAN_0);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_SCAN_1);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_SCAN_2);
    HAL_vFileClear(FILE_NAME_HW_PARA_TJC_PIC_PAY_SCAN_3);

    LYY_LOGD("==================  delete end ");
}
/**
 * @brief 更新屏幕型号字符串。
 * @param[in] type 零结尾型号；NULL 或长度达到 64 字节时忽略。
 */
void set_tjc_Screen(char *type)
{
    if (type == NULL || strlen(type) >= sizeof(lyy_screen)) return;
    memset(lyy_screen,0x00,64);
    memcpy(lyy_screen,type,strlen(type));
    LYY_LOGD("set_tjcScreen:%s",lyy_screen);

}
/**
 * @brief 更新屏幕方向或布局类型字符串。
 * @param[in] type 零结尾类型；NULL 或长度达到 64 字节时忽略。
 */
void set_tjc_screen_type(char *type)
{
    if (type == NULL || strlen(type) >= sizeof(lyy_screenType)) return;
    memset(lyy_screenType,0x00,64);
    memcpy(lyy_screenType,type,strlen(type));
    LYY_LOGD("set_tjcScreenType:%s",lyy_screenType);
}
/**
 * @brief 兼容旧命名的屏幕型号设置入口。
 * @param[in] type 型号字符串，约束同 set_tjc_Screen。
 */
void set_tjcScreen(char *type) { set_tjc_Screen(type); }
/**
 * @brief 兼容旧命名的屏幕布局类型设置入口。
 * @param[in] type 类型字符串，约束同 set_tjc_screen_type。
 */
void set_tjcScreenType(char *type) { set_tjc_screen_type(type); }
/**
 * @brief 
 * picState
 * @param arg 
 */
void tjc_setProcess(unsigned char newState)
{
    if(picVair.process != newState)
    {
       picVair.process = newState;
    }
}
/**
 * @brief 
 * picState
 * @param arg 
 */
unsigned char tjc_getProcess(void)
{
    return  picVair.process;
}

/**
 * @brief 
 * 页面获取
 */
void getTjcPicUrl_page(char *page,unsigned char wallet)
{

    PAY_PARAM_DEF *pay = get_pay_param();

    LYY_LOGD("getTjcPicUrl_page");
    // 创建根 cJSON 对象，它是数组类型
    cJSON *root = cJSON_CreateObject();
    if(!root) {

        return ;
    }

    // 添加 device_type
    cJSON_AddStringToObject(root, "device_type", lyy_screen);
    // 添加 screen_type
    cJSON_AddStringToObject(root, "screen_type", (const char *)lyy_screenType);

    // 创建 must 对象
    cJSON *must = cJSON_CreateObject();  

    if(!must) {

        cJSON_Delete(root);
        return ;
    }

    cJSON_AddStringToObject(must, "container", (const char *)page);
    cJSON_AddStringToObject(must, "region", (const char *)pay->channel[wallet].region);
    // 将 must 对象添加到根对象
    cJSON_AddItemToObject(root, "must", must);

    // 创建 match 对象
    cJSON *match = cJSON_CreateObject();
    if(!match) {

        cJSON_Delete(root);
        return ;
    }

    cJSON_AddStringToObject(match, "channel_code", (const char *)pay->channel[wallet].channel_code);
    cJSON_AddStringToObject(match, "wallet_code", (const char *)pay->channel[wallet].code);

    // 将 match 对象添加到根对象
    cJSON_AddItemToObject(root, "match", match);

    // 将 cJSON 结构体转换为 JSON 字符串并打印输出
    char *json_string = cJSON_PrintUnformatted(root);

    LYY_LOGD("%s", json_string);

    if(!json_string) {

        cJSON_Delete(root);
        return ;
    }
        
    cJSON *payload = cJSON_CreateObject();

    if(!payload) {

        cJSON_Delete(root);
        free(json_string);
        return ;
    }  

    cJSON_AddStringToObject(payload, "payload", json_string);

    thing_event_post(payload, "load_container_layout");
   
    cJSON_Delete(root);
    free(json_string);

}


/**
 * @brief 按渠道逐个请求图片 URL，随后进入图片更新阶段。
 * @note 每渠道最多等待 10000 ms，超时将该渠道两张图片标记为更新失败；元数据为空时退出至空闲。
 */
void getTjcPicUrl(void)
{


    if(!g_pxTjcPicFlashInfo.pucData) {

        tjc_setProcess(TJC_PIC_NULL);
        LYY_LOGE("### malloc err");
        return ;
    }

    unsigned char result;

    picVair.startGetPic = 0;

    for(; picVair.startGetPic < g_pxTjcPicFlashInfo.ucWalletNumSum; picVair.startGetPic++) {

        LYY_LOGD("picVair.startGetPic===================[%d]",picVair.startGetPic);
        getTjcPicUrl_page("pay",picVair.startGetPic);
        
        g_SemSta = 1;
        result = xSemaphoreTake(hw_httpGetPicUrlSem, pdMS_TO_TICKS(10000));
        g_SemSta = 0;

        if(result == 0)  {

            //如果获取最新的失败，则更新失败
            picVair.picState[(picVair.startGetPic*PIC_INFO_MAX)]    = TJC_PIC_RESULT_UPDATA_FAIL;
            picVair.picState[(picVair.startGetPic*PIC_INFO_MAX)+1]  = TJC_PIC_RESULT_UPDATA_FAIL;
        }

    }


    tjc_setProcess(TJC_PIC_UPDATE_URL);

}

/**
 * @brief 解析当前请求渠道的图片布局应答并更新两张图片状态。
 * @param[in] params valuestring 为布局 JSON 字符串的节点。
 * @retval -1 参数、缓存或当前渠道下标无效，或 JSON 解析失败。
 * @retval -2 缺少 device_type。
 * @retval -3 缺少 screen_type。
 * @retval -4 缺少 components。
 * @retval -5 缺少 selected。
 * @retval -10 wallet_url 缺失、无字符串值或过长。
 * @retval -11 scan_url 缺失、无字符串值或过长。
 * @retval 0 更新完成并按等待标志释放 URL 信号量。
 * @note URL 变化或原文件大小为零则标记待下载，否则标记待传输；中途失败不回滚已更新的前一张图片。
 */
int tjc_recv_pic_url(cJSON* params)
{


    if (params == NULL || params->valuestring == NULL || g_pxTjcPicFlashInfo.pucData == NULL ||
        picVair.startGetPic >= g_pxTjcPicFlashInfo.ucWalletNumSum) return -1;
    LYY_LOGD("payload->valuestring: %s\n", params->valuestring);
    cJSON *payload_json = cJSON_Parse(params->valuestring);
    
    if (payload_json == NULL) {
        LYY_LOGD("tjc_recv_pic_url payload_json is null");
        return -1;
    }
    
      // 获取并打印 payload 中的各个字段
    cJSON *device_type = cJSON_GetObjectItem(payload_json, "device_type");
    if(device_type == NULL) 
    {
        cJSON_Delete(payload_json);
        return -2;
    }

    LYY_LOGD("device_type: %s\n", device_type->valuestring);
    cJSON *screen_type = cJSON_GetObjectItem(payload_json, "screen_type");
    if(screen_type == NULL) 
    {
        cJSON_Delete(payload_json);
        return -3;
    }

    LYY_LOGD("screen_type: %s\n", screen_type->valuestring);
    cJSON *components = cJSON_GetObjectItem(payload_json, "components");
    if(components == NULL)     {
        cJSON_Delete(payload_json);
        return -4;
    }

    cJSON *selected = cJSON_GetObjectItem(payload_json, "selected");
    if(selected == NULL)     {
        cJSON_Delete(payload_json);
        return -5;
    }

    LYY_LOGD("======================================================");

    // 判断图片url是否相同
    

    uint32_t ulUrlAdd =  (picVair.startGetPic * PIC_INFO_MAX);
                        
    INFOR_PIC_INFO *pxTjcInfo = 
        (INFOR_PIC_INFO *)(g_pxTjcPicFlashInfo.pucData + ulUrlAdd * sizeof(INFOR_PIC_INFO));

    LYY_LOGD("pic1 : url_add = [%02x]", ulUrlAdd);



    // 判断读url是否跟新
    cJSON *wallet_url = cJSON_GetObjectItem(components, "wallet_url");

    if(wallet_url == NULL)     {

        LYY_LOGE("wallet null");
        cJSON_Delete(payload_json);
        return -10;
    }

    if (wallet_url->valuestring == NULL || strlen(wallet_url->valuestring) >= sizeof(pxTjcInfo->url)) {
        cJSON_Delete(payload_json); return -10;
    }
    if(((wallet_url->valuestring != NULL) && 
           strcmp((const char *)&pxTjcInfo->url[0],wallet_url->valuestring)) || 
           pxTjcInfo->filesize == 0x00)
    {

        LYY_LOGD("=====%d   %s", pxTjcInfo->filesize, pxTjcInfo->url);
        picVair.picState[picVair.startGetPic * PIC_INFO_MAX] = TJC_PIC_STATE_UPDATA;


        memset(     (char *)&pxTjcInfo->filesize, 
                    0x00, 
                    sizeof(INFOR_PIC_INFO));
                    
        memcpy(    (char *)&pxTjcInfo->url[0], 
                    wallet_url->valuestring,
                    strlen(wallet_url->valuestring));

        LYY_LOGD("pic1 : lyy_file_write_pic_ex addr = [%02x] url = [%s]",ulUrlAdd ,wallet_url->valuestring);  



    }
    else
    {
        // 查询url都进行传输
        picVair.picState[picVair.startGetPic*PIC_INFO_MAX] = TJC_PIC_STATE_TRAN;
        LYY_LOGD("pic1 :  lyy_file_write_pic_ex url_add = TJC_PIC_STATE_TRAN");
    }



    ulUrlAdd =  (picVair.startGetPic * PIC_INFO_MAX + 1);

    pxTjcInfo = 
        (INFOR_PIC_INFO *)(g_pxTjcPicFlashInfo.pucData + ulUrlAdd * sizeof(INFOR_PIC_INFO));
                        
    LYY_LOGD("pic1 : url_add = [%02x]", ulUrlAdd);



    cJSON *scan_url = cJSON_GetObjectItem(components, "scan_url");

    if(scan_url == NULL)     {
        cJSON_Delete(payload_json);
        return -11;
    }

    if (scan_url->valuestring == NULL || strlen(scan_url->valuestring) >= sizeof(pxTjcInfo->url)) {
        cJSON_Delete(payload_json); return -11;
    }
    if(((scan_url->valuestring != NULL) 
                && strcmp((const char *)&pxTjcInfo->url[0],scan_url->valuestring))  
                || pxTjcInfo->filesize == 0x00)
    {

        picVair.picState[picVair.startGetPic*PIC_INFO_MAX+1] = TJC_PIC_STATE_UPDATA;

        memset(     (char *)&pxTjcInfo->filesize, 
                    0x00, 
                    sizeof(INFOR_PIC_INFO));
                    
        memcpy(    (char *)&pxTjcInfo->url[0], 
                    scan_url->valuestring,
                    strlen(scan_url->valuestring));

        LYY_LOGD("pic2 : lyy_file_write_pic_ex addr = [%02x] url = [%s]",ulUrlAdd ,scan_url->valuestring);  


    }else
    {
        // 查询url都进行传输
        picVair.picState[picVair.startGetPic*PIC_INFO_MAX+1] = TJC_PIC_STATE_TRAN;
        LYY_LOGD("pic2 :lyy_file_write_pic_ex url_add = TJC_PIC_STATE_TRAN");
    }



    if(g_SemSta)
    {
        xSemaphoreGive(hw_httpGetPicUrlSem);  
        g_SemSta = 0x00;
    }
      
    cJSON_Delete(payload_json);

    return 0;

}


static void APP_prvHttpDataRvCd(char *pcData, uint32_t ulLen)
{

    int lRet = HAL_vFileWriteOffset(g_pcPath, 
                                    (uint8_t *)pcData, 
                                    ulLen,
                                    g_ulWriteOffset);
    
    if(lRet < 0 || lRet != (int)ulLen) {

        LYY_LOGE("###  write fail");
        return ;
    } 

    LYY_LOGD(">>> write datalen : [%d]   offset :[%d]", ulLen, g_ulWriteOffset);

    g_ulWriteOffset += ulLen;

}



static void APP_prvHttpRes(uint8_t ucRes, uint32_t ulLen)
{
    (void)ucRes;

    LYY_LOGD("HTTP RECV DATA len: [%d]", ulLen);

    if(HTTP_GET_SUCCESS == ucRes) {

        if(g_ulWriteOffset == ulLen) {

            LYY_LOGD("write succes :[%s]", g_pcPath);

        }else{
            
            g_ulWriteOffset = 0x00;
        }

    }else{

        g_ulWriteOffset = 0x00;
    }

    if(g_SemSta) {

        xSemaphoreGive(hw_httpGetPicUrlSem);  
        g_SemSta = 0x00;
    }

}

/**
 * @brief 下载当前渠道指定类型图片并保存文件及元数据。
 * @param[in] type 图片类型下标 PIG_WALLET 或 PIG_SCAN。
 * @note 调用前须准备有效元数据、当前渠道及信号量；每次清除旧文件和残留信号，最多等待 15000 ms，写入长度为零时再重试一次。
 */
void lyy_http_get_xiPic(unsigned char type) 
{

    int ucRetry = 0x00;


    uint32_t ulUrlAdd =  (picVair.startGetPic * PIC_INFO_MAX + type);

    INFOR_PIC_INFO *pxTjcInfo = 
        (INFOR_PIC_INFO *)(g_pxTjcPicFlashInfo.pucData + ulUrlAdd * sizeof(INFOR_PIC_INFO));

http_retry : 

    memset(g_pcPath, 0x00, sizeof(g_pcPath));

    picture_cache_path(g_pcPath, sizeof(g_pcPath), picVair.startGetPic, type);


    LYY_LOGD("pic : type : [%d] url_add = [%d] url : [%s] pcPath : [%s]", 
                                        type,  ulUrlAdd, pxTjcInfo->url, g_pcPath);    

    HAL_vFileClear(g_pcPath);

    g_ulWriteOffset = 0x00;

    xSemaphoreTake(hw_httpGetPicUrlSem, 0);
    g_SemSta = 1;
    HAL_vHttpGet(   (const char *)pxTjcInfo->url, 
                    APP_prvHttpDataRvCd, 
                    APP_prvHttpRes);                         

    int result = xSemaphoreTake(hw_httpGetPicUrlSem, pdMS_TO_TICKS(15000));
    g_SemSta = 0;

    if(result) {

        if(g_ulWriteOffset) {

            pxTjcInfo->filesize = g_ulWriteOffset;

            HAL_vFileWrite( FILE_NAME_HW_PARA_TJC_PIC_DATA_INFO, 
                            g_pxTjcPicFlashInfo.pucData, 
                            sizeof(INFOR_PIC_INFO) * g_pxTjcPicFlashInfo.ucWalletNumSum * 2);    
              
            picVair.picState[((picVair.startGetPic*PIC_INFO_MAX)+picVair.startGetPicType)] = TJC_PIC_STATE_TRAN;

        }else {

            picVair.picState[((picVair.startGetPic*PIC_INFO_MAX)+picVair.startGetPicType)] = TJC_PIC_RESULT_TRAN_FAIL;
        }

    }else{

        picVair.picState[((picVair.startGetPic*PIC_INFO_MAX)+picVair.startGetPicType)] = TJC_PIC_RESULT_TRAN_FAIL;
    }

    if(!g_ulWriteOffset && ucRetry++ < 1) {

        vTaskDelay(1000 / portTICK_PERIOD_MS);
        goto http_retry;
    }


}



/**
 * @brief 检查当前渠道的图片缓存并下载所需文件。
 * @details URL 已匹配的缓存仍须与元数据长度一致。缓存缺失或长度不符时，
 *          重新下载该图片。处理完各渠道后进入串口屏传输阶段。
 */
void getTjcPicUpdata(void)
{


    if(!g_pxTjcPicFlashInfo.pucData) {

        tjc_setProcess(TJC_PIC_NULL);
        LYY_LOGE("### malloc err");
        return ;
    }

    picVair.startGetPic = 0;
    
    for(; picVair.startGetPic <  g_pxTjcPicFlashInfo.ucWalletNumSum; picVair.startGetPic++)
    {

        LYY_LOGD("picVair.startGetPic===================[%d]",picVair.startGetPic);

        // 有没有需要更新的
        for(picVair.startGetPicType = PIG_WALLET; picVair.startGetPicType <= PIG_SCAN; picVair.startGetPicType++)
        {

            unsigned int index = picVair.startGetPic * PIC_INFO_MAX + picVair.startGetPicType;
            if (picVair.picState[index] == TJC_PIC_STATE_TRAN) {
                char path[20];
                picture_cache_path(path, sizeof(path), picVair.startGetPic, picVair.startGetPicType);
                const INFOR_PIC_INFO *images = (const INFOR_PIC_INFO *)g_pxTjcPicFlashInfo.pucData;
                long actual = HAL_lFileSize(path);
                if (actual < 0 || (unsigned long)actual != images[index].filesize) {
                    LYY_LOGW("Screen picture cache incomplete: %s, got %ld/%u",
                             path, actual, images[index].filesize);
                    picVair.picState[index] = TJC_PIC_STATE_UPDATA;
                }
            }
            if (picVair.picState[index] != TJC_PIC_STATE_UPDATA)
                continue;

            vTaskDelay(300 / portTICK_PERIOD_MS);
            
            // 调用http获取图
            lyy_http_get_xiPic(picVair.startGetPicType);

        }
    }

    tjc_setProcess(TJC_PIC_TRAN_INFO);

}


/**
 * @brief 
 * taojingchi串口屏控件txt修改
 * @param objName 对象名称
 * @param txt 对象内容
 */
void lyy_pic_objTxt(char *objName, char *txt)
{
    if(objName == NULL) return;
    if(txt == NULL) return;
    
    char CmdBuff[LYY_DISPLAY_BUFF_MAX] = {0x00,};
    memset(CmdBuff, 0, LYY_DISPLAY_BUFF_MAX);
    sprintf(CmdBuff, "%s.txt=\"%s\"\xff\xff\xff", objName, txt);

    //需要更换成
    lyy_display_send((unsigned char *)CmdBuff, strlen(CmdBuff));


}
/**
 * @brief 将待传输图片依次发送到屏幕 RAM 文件。
 * @note 分包进度通过信号量续期。20 秒未收到进度或接收失败时退出并重传一次，
 *       只有末包后的完成应答会将该图片标记为传输成功。
 */
void getTjcPicTran(void)
{
    

    if(!g_pxTjcPicFlashInfo.pucData) {

        tjc_setProcess(TJC_PIC_NULL);
        LYY_LOGE("### malloc err");
        return ;
    }

    unsigned char result;
    picVair.startGetPic = 0;

    char file_name[20];

    uint32_t ulUrlAdd;

    for(; picVair.startGetPic <  g_pxTjcPicFlashInfo.ucWalletNumSum;picVair.startGetPic++)
    {
        for(picVair.startGetPicType = PIG_WALLET;picVair.startGetPicType <= PIG_SCAN;picVair.startGetPicType++)
        {
            // 有没有需要更新的
            if(picVair.picState[((picVair.startGetPic*PIC_INFO_MAX)+picVair.startGetPicType)] != TJC_PIC_STATE_TRAN) {

                continue; 
            }
                

           ulUrlAdd =  (picVair.startGetPic * PIC_INFO_MAX + picVair.startGetPicType);

           INFOR_PIC_INFO *pxTjcInfo = 
                (INFOR_PIC_INFO *)(g_pxTjcPicFlashInfo.pucData + ulUrlAdd * sizeof(INFOR_PIC_INFO));                  


            lyy_pic_objTxt("body", "Image updates : start");                   // 使用屏幕刷新指令
            vTaskDelay(1000 / portTICK_PERIOD_MS);

            // 拼接文件名称
            memset(file_name,0x00,20);
            sprintf(file_name,"filename_%d",(picVair.startGetPic * PIC_INFO_MAX + picVair.startGetPicType));
            
            LYY_LOGD("========================  url_temp.filesize : [%d]  ulUrlAdd :[%d]" , pxTjcInfo->filesize, ulUrlAdd);
            picVair.picState[ulUrlAdd] = TJC_PIC_RESULT_TRAN_FAIL;
            for (unsigned int attempt = 0; attempt < 2; ++attempt) {
                xSemaphoreTake(hw_httpTranPicUrlSem, 0);
                if (!lyy_display_sendPic("ram", file_name, pxTjcInfo->filesize)) break;

                do {
                    result = xSemaphoreTake(hw_httpTranPicUrlSem, pdMS_TO_TICKS(20000));
                } while (result && lyy_display_getSendPicStatus() == LYY_SENDPIC_TRANS);

                if (result && picTransferComplete) {
                    picVair.picState[ulUrlAdd] = TJC_PIC_RESULT_TRAN_SUCCESS;
                    break;
                }
                LYY_LOGE("Screen picture transfer failed: %s, packet %u/%u",
                         file_name, sendPic.sendPack, sendPic.totalPack);
                lyy_display_setSendPicStatus(LYY_SENDPIC_NORMAL);
                lyy_display_sendExitTrans();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }
        }
    }
    picVair.process = TJC_PIC_NULL;

}


/**
 * @description: 获取屏幕状态
 * 
 * @return {unsigned char } 屏幕状态
 */
unsigned char lyy_display_getSendPicStatus(void)
{
    return sendPic.state;
}
/**
 * @description: 设置屏幕状态
 * 
 * @param [in]    Status 状态
 * 
 * @return {unsigned char } 状态值
 */

unsigned char lyy_display_setSendPicStatus(unsigned char Status)
{
    if(Status == LYY_SENDPIC_TRANS)
    {
        sendPic.state = LYY_SENDPIC_TRANS;
    }else   // 其他状态则切换成普通状态
    {
        sendPic.state = LYY_SENDPIC_NORMAL;
    }

    return sendPic.state;
}

/**
 * @description: 发送退出传输模式
 * 
 * @return {int } 成功
 */
int lyy_display_sendExitTrans(void)
{
    // 发送传输退出
    unsigned char CmdSetStatus[] = {0x3a,0xa1,0xbb,0x44,0x7f,0xff,0xfe,0x00,0xff,0xff,0x00,0x00};

    lyy_display_send(CmdSetStatus,12);

    return 1;
}


/**
 * @brief 初始化分包状态并向串口屏发送 twfile 命令。
 * @param[in] addr 屏幕存储路径，如 ram 或 sd0。
 * @param[in] fileName 图片文件名，不带 .xi 后缀，须适合传输状态的文件名缓冲区。
 * @param[in] fileSize 图片字节数，须能用 16 位包号覆盖。
 * @retval 1 已提交传输命令，随后等待屏幕应答。
 * @retval 0 路径或文件名指针为空，或者文件长度超出可传输范围。
 * @note 先进入传输状态再发送 twfile，避免漏掉先到达的 FE 确认。
 */
int lyy_display_sendPic(char *  addr, char * fileName, unsigned int  fileSize)
{
    int res = 1;
    if(addr == NULL) return 0;
    if(fileName == NULL) return 0;
    if (fileSize == 0 || fileSize > (UINT16_MAX + 1U) * LYY_SEND_PIC_PACKET_SIZE) return 0;

    char CmdBuff[LYY_DISPLAY_BUFF_MAX] = {0x00,};
    
    memset(CmdBuff,0,LYY_DISPLAY_BUFF_MAX);

    sprintf(CmdBuff,"twfile \"%s/%s.xi\",%d\xff\xff\xff",addr,fileName,fileSize);

    memset(sendPic.fileName,0x00,20);

    memcpy(sendPic.fileName,fileName,strlen(fileName));
    sendPic.fileSize = fileSize;
    sendPic.sendPack = 0;
    sendPic.retries = 0;
    sendPic.packetSent = 0;
    picTransferComplete = 0;

    sendPic.totalPack = (fileSize/LYY_SEND_PIC_PACKET_SIZE);
    if(fileSize%LYY_SEND_PIC_PACKET_SIZE != 0)
    {
        sendPic.totalPack++;
    }

    LYY_LOGD("====================================================");
    LYY_LOGD("====================================================");
    LYY_LOGD("  fileName = %s",sendPic.fileName);
    LYY_LOGD("  fileSize = %d",sendPic.fileSize);
    LYY_LOGD("  sendPic.totalPack = %d",sendPic.totalPack);
    LYY_LOGD("====================================================");
    LYY_LOGD("====================================================");

    lyy_display_setSendPicStatus(LYY_SENDPIC_TRANS);
    lyy_display_send((unsigned char *)CmdBuff,strlen((const char *)CmdBuff));

    return res;
}

/**
 * @brief 处理 STM32 转发的串口屏图片确认，包括同帧中的多个确认。
 * @param[in] pack 屏幕返回的原始字节，不需要 NUL 结尾。
 * @param[in] len 有效字节数。
 * @retval 1 至少识别一条图片确认。
 * @retval 0 缓冲区为空、当前未传图或未识别出图片确认。
 * @details FE 开始传输，05 推进分包，04 重发当前包，06 表示启动失败。
 *          FD 仅在末包发出后记为完成，进度和终止结果均通过信号量唤醒等待任务。
 */
int lyy_display_recvPic(unsigned char *pack, unsigned int len)
{
    if (!pack || !len || lyy_display_getSendPicStatus() != LYY_SENDPIC_TRANS) return 0;

    int handled = 0;
    while (len && lyy_display_getSendPicStatus() == LYY_SENDPIC_TRANS) {
        uint8_t status = *pack++;
        len--;
        if (status == 0xFF && handled) continue;
        if (status == 0xFE || status == 0xFD || status == 0x06) {
            if (len < 3 || memcmp(pack, "\xff\xff\xff", 3) != 0) break;
            pack += 3;
            len -= 3;
        } else if (status != 0x04 && status != 0x05) {
            break;
        }
        handled = 1;

        if (status == 0xFD) {
            // 退出传输也可能收到 FD，只有末包发出后才接受完成确认。
            if (!sendPic.packetSent || sendPic.sendPack + 1 != sendPic.totalPack) continue;
            picTransferComplete = 1;
        } else if (status != 0x06) {
            if (status == 0x05) {
                if (!sendPic.packetSent || sendPic.sendPack + 1 >= sendPic.totalPack) continue;
                sendPic.sendPack++;
                sendPic.retries = 0;
            } else if (status == 0x04 || sendPic.packetSent) {
                if (!sendPic.packetSent) continue;
                if (++sendPic.retries > 3) goto transfer_done;
            }
            if (lyy_display_pick(sendPic.sendPack)) {
                sendPic.packetSent = 1;
                xSemaphoreGive(hw_httpTranPicUrlSem);
                continue;
            }
        }

transfer_done:
        lyy_display_setSendPicStatus(LYY_SENDPIC_NORMAL);
        xSemaphoreGive(hw_httpTranPicUrlSem);
    }
    return handled;
}

/**
 * @brief 从 Flash 图片缓存读取并发送指定分包。
 * @param[in] packet 从 0 开始的分包序号。
 * @retval 1 已将完整的屏幕分包交给 D6/01 发送入口。
 * @retval 0 包号越界或缓存读取长度不足。
 * @note 数据长度按文件剩余字节计算，整除分包大小的末包仍发送完整数据。
 */
int lyy_display_pick(unsigned int packet)
{
    if (packet >= sendPic.totalPack) return 0;

    unsigned int offset = packet * LYY_SEND_PIC_PACKET_SIZE;
    unsigned int readSize = sendPic.fileSize - offset;
    if (readSize > LYY_SEND_PIC_PACKET_SIZE) readSize = LYY_SEND_PIC_PACKET_SIZE;

    unsigned char fileBuff[LYY_SEND_PIC_HEADER_SIZE + LYY_SEND_PIC_PACKET_SIZE] = {
        0x3a, 0xa1, 0xbb, 0x44, 0x7f, 0xff, 0xfe, 0x00
    };
    fileBuff[8] = packet & 0xff;
    fileBuff[9] = packet >> 8;
    fileBuff[10] = readSize & 0xff;
    fileBuff[11] = readSize >> 8;

    char path[20];
    picture_cache_path(path, sizeof(path), picVair.startGetPic, picVair.startGetPicType);
    int bytes = HAL_vFileReadOffset(path, fileBuff + LYY_SEND_PIC_HEADER_SIZE, readSize, offset);
    if (bytes != (int)readSize) {
        LYY_LOGE("Screen picture short read: %s, offset %u, got %d/%u", path, offset, bytes, readSize);
        return 0;
    }

    lyy_display_send(fileBuff, readSize + LYY_SEND_PIC_HEADER_SIZE);
    return 1;
}
/**
 * @description: 设置控件外部图片
 * 
 * @param [in]    name 名称
 * @param [in]    path  路径     
 * 
 * @return {int } 1成功/0失败
 */
int lyy_display_setPicPath(char * name,char *  path)
{
    if(name == NULL) return 0;
    if(path == NULL) return 0;

    char CmdBuff[LYY_DISPLAY_BUFF_MAX] = {0x00,};

    memset(CmdBuff,0,LYY_DISPLAY_BUFF_MAX);

    sprintf(CmdBuff,"%s.path=\"ram/%s.xi\"\xff\xff\xff",name,path);

    lyy_display_send((unsigned char *)CmdBuff,strlen((const char *)CmdBuff));

    return 1;
}
/**
 * @brief 将指定钱包已传输成功的图片绑定到对应屏幕控件。
 * @param[in] page 页面字符串，目前仅含 pay 的页面执行图片绑定。
 * @param[in] walletNum 从 1 开始的钱包序号。
 * @retval 0 page 为 NULL 或钱包序号为 0。
 * @retval -1 select、result、working 或 warn 页面不提供图片。
 * @retval -2 钱包超出已有数量或页面不受支持。
 * @retval -3 两张图片均未成功传输至屏幕。
 * @retval 1 已请求绑定成功传至屏幕的图片。
 * @note 图片索引为 (walletNum-1)*PIC_INFO_MAX，wallet 和 wallet_type 各自依据传输结果绑定。
 */
int lyy_display_showpic(char *page,unsigned char walletNum)
{
    if(page == NULL) return 0;
    if(walletNum == 0) return 0;

    LYY_LOGD("lyy_display_showpic1 %s:%d",page,walletNum);
    LYY_LOGD("lyy_display_showpic2 %s:%d",LYY_PIC_PAY_PAGE_TYPE,g_pxTjcPicFlashInfo.ucWalletNumSum);
    // 选择页面
    if(strncmp(page, LYY_PIC_PACKAGES_PAGE_TYPE, strlen(LYY_PIC_PACKAGES_PAGE_TYPE)) == 0)
    {
        return -1;
    }else if (strstr(page,LYY_PIC_PAY_PAGE_TYPE) != NULL) //
    {

        // 该页是否钱包数是否正确
        if(walletNum > CHANNELINFO_MAX || walletNum > g_pxTjcPicFlashInfo.ucWalletNumSum)
        {
            LYY_LOGD("walletInfo.walletNum =========[%d]",g_pxTjcPicFlashInfo.ucWalletNumSum);
            return -2;
        }

        char file_name[20];
        unsigned int index = (walletNum - 1) * PIC_INFO_MAX;
        int shown = 0;
        if (picVair.picState[index] == TJC_PIC_RESULT_TRAN_SUCCESS) {
            snprintf(file_name, sizeof(file_name), "filename_%u", index);
            lyy_display_setPicPath("wallet", file_name);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            lyy_tjc_objHide("wallet", 1);
            shown = 1;
        }
        if (picVair.picState[index + 1] == TJC_PIC_RESULT_TRAN_SUCCESS) {
            snprintf(file_name, sizeof(file_name), "filename_%u", index + 1);
            lyy_display_setPicPath("wallet_type", file_name);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            lyy_tjc_objHide("wallet_type", 1);
            shown = 1;
        } else {
            // 原页切换钱包时，防止显示上一个渠道的扫码提示图。
            lyy_tjc_objHide("wallet_type", 0);
        }
        return shown ? 1 : -3;

    }else if (strncmp(page, LYY_PIC_RESULT_PAGE_TYPE, strlen(LYY_PIC_RESULT_PAGE_TYPE)) == 0)
    {
        return -1;
    }else if (strncmp(page, LYY_PIC_WORKING_PAGE_TYPE, strlen(LYY_PIC_WORKING_PAGE_TYPE)) == 0)
    {
        return -1;
    }else if (strncmp(page, LYY_PIC_WARN_PAGE_TYPE, strlen(LYY_PIC_WARN_PAGE_TYPE)) == 0)
    {
        return -1;
    }else
    {
        return -2;
    }
}



/**
 * @brief 同步推进图片 URL、下载及传输状态，空闲后返回。
 * @param[in] arg 未使用，可为 NULL。
 * @note 传输阶段结束调用结果回调；此函数在屏幕初始化任务中直接调用，不自行创建或删除任务。
 */
void thread_tjcPicDealThread(void *arg)
{
    
    (void)arg;

    while(1)
    {
        switch (picVair.process)
        {
            case TJC_PIC_GET_URL:       // 获取图片url
                LYY_LOGD("=============TJC_PIC_GET_URL===================");
                vTaskDelay(2000 / portTICK_PERIOD_MS); 
                getTjcPicUrl();
                break;
            case TJC_PIC_UPDATE_URL:    // 获取图片数据
                LYY_LOGD("=============TJC_PIC_UPDATE_URL===================");
                getTjcPicUpdata();
                break;
            case TJC_PIC_TRAN_INFO:    // 传输图片
                LYY_LOGD("===================TJC_PIC_TRAN_INFO===================");
                getTjcPicTran();
                if(tjc_pic_result_cb != NULL)
                {
                	tjc_pic_result_cb(&picVair);
                }
                break;
            default:
                LYY_LOGD("===================Ending===================");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                return;
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

}




void lyy_picInit(void)
{

    hw_httpGetPicUrlSem  = xSemaphoreCreateBinary();
    hw_httpTranPicUrlSem = xSemaphoreCreateBinary();  
}


