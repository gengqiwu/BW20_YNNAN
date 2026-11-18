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
#include "thing_mqtt.h"

#include "USER_Config.h"



INFOR_PIC_VAIR picVair;


char lyy_screenType[64] = LYY_SCREEN_TYPC_00;
char lyy_screen[64] = LYY_SCREEN_DEVICE_4880x243;


LYY_SENDPIC sendPic;
static uint8_t g_SemSta;


static uint32_t g_ulWriteOffset;
static char g_pcPath[64];

SemaphoreHandle_t hw_httpGetPicUrlSem  = NULL;
SemaphoreHandle_t hw_httpTranPicUrlSem = NULL;


static INFOR_PIC_Flash_INFO g_pxTjcPicFlashInfo;

void (*tjc_pic_result_cb)(INFOR_PIC_VAIR *) = NULL;

/**
 * @description: 发送
 * 
 * @param [in]    data 数据  
 * @param [in]    len 长度 
 * 
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
 * @brief 
 * 图片信息保存以及启动
 * @param arg 
 *
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
 * @brief 
 * 设置淘晶驰屏幕类型
 * @param [char *] type 
 */
void set_tjc_Screen(char *type)
{
    if (type == NULL || strlen(type) >= sizeof(lyy_screen)) return;
    memset(lyy_screen,0x00,64);
    memcpy(lyy_screen,type,strlen(type));
    LYY_LOGD("set_tjcScreen:%s",lyy_screen);

}
/**
 * @brief 
 * 设置淘晶驰屏幕类型
 * @param arg 
 */
void set_tjc_screen_type(char *type)
{
    if (type == NULL || strlen(type) >= sizeof(lyy_screenType)) return;
    memset(lyy_screenType,0x00,64);
    memcpy(lyy_screenType,type,strlen(type));
    LYY_LOGD("set_tjcScreenType:%s",lyy_screenType);
}
void set_tjcScreen(char *type) { set_tjc_Screen(type); }
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
 * @brief 
 * 获取tjc图片的url
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
 * @brief 
 * 接收tjc图片的url
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
 * @description: 发送http获取图片
 * @param {*}
 * @return {*}
 */
void lyy_http_get_xiPic(unsigned char type) 
{

    int ucRetry = 0x00;


    uint32_t ulUrlAdd =  (picVair.startGetPic * PIC_INFO_MAX + type);

    INFOR_PIC_INFO *pxTjcInfo = 
        (INFOR_PIC_INFO *)(g_pxTjcPicFlashInfo.pucData + ulUrlAdd * sizeof(INFOR_PIC_INFO));

http_retry : 

    memset(g_pcPath, 0x00, sizeof(g_pcPath));

    sprintf( g_pcPath, 
            picVair.startGetPicType == PIG_WALLET ? "PAY_WALLET_%d" : "PAY_SCAN_%d", 
            picVair.startGetPic);


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
 * @brief 
 * http跟新图片
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

            if(picVair.picState[((picVair.startGetPic*PIC_INFO_MAX) + picVair.startGetPicType)] != TJC_PIC_STATE_UPDATA) 
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
 * @brief 
 * tjc图片传输
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
            xSemaphoreTake(hw_httpTranPicUrlSem, 0);
            g_SemSta = 1;
            lyy_display_sendPic("ram",file_name, pxTjcInfo->filesize);

            
            result = xSemaphoreTake(hw_httpTranPicUrlSem, pdMS_TO_TICKS(20000));
            g_SemSta = 0;

            if(result == 0)
            {
                picVair.picState[((picVair.startGetPic*PIC_INFO_MAX)+picVair.startGetPicType)] = TJC_PIC_RESULT_TRAN_FAIL;
                lyy_display_setSendPicStatus(LYY_SENDPIC_NORMAL);
                lyy_display_sendExitTrans();
                vTaskDelay(1000 / portTICK_PERIOD_MS);
            }else
            {
                picVair.picState[((picVair.startGetPic*PIC_INFO_MAX)+picVair.startGetPicType)] = TJC_PIC_RESULT_TRAN_SUCCESS;
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
 * @description: 外部图片传输指令
 * 
 * @param [in]    addr 地址路径(ram/sd0)
 * @param [in]    fileName 文件
 * @param [in]    fileSize  文件大小    
 * 
 * @return {int } 1成功/0失败
 */
int lyy_display_sendPic(char *  addr, char * fileName, unsigned int  fileSize)
{
    int res = 1;
    if(addr == NULL) return 0;
    if(fileName == NULL) return 0;
    if(fileSize == 0) return 0;

    char CmdBuff[LYY_DISPLAY_BUFF_MAX] = {0x00,};
    
    memset(CmdBuff,0,LYY_DISPLAY_BUFF_MAX);

    sprintf(CmdBuff,"twfile \"%s/%s.xi\",%d\xff\xff\xff",addr,fileName,fileSize);

    memset(sendPic.fileName,0x00,20);

    memcpy(sendPic.fileName,fileName,strlen(fileName));
    sendPic.fileSize = fileSize;
    sendPic.sendPack = 0;       // 从第0包开始

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

    lyy_display_send((unsigned char *)CmdBuff,strlen((const char *)CmdBuff));

    lyy_display_setSendPicStatus(LYY_SENDPIC_TRANS);

    return res;
}

/**
 * @description: 外部图片传输接收处理
 * 
 * @param [in]    pack 接收返回数据
 * @param [in]    len  数据大小  
 * 
 * @return {unsigned char } 1成功/0失败
 */
int lyy_display_recvPic(unsigned char * pack,unsigned int  len)
{
    if(pack == NULL) return 0;
    if(len == 0) return 0;

    if(lyy_display_getSendPicStatus() != LYY_SENDPIC_TRANS ) return 0;

    #define PIC_CMD_FILE_SUCESS "\xfe\xff\xff\xff"
    #define PIC_CMD_FILE_FAIL "\x06\xff\xff\xff"
    #define PIC_CMD_FILE_END "\xfd\xff\xff\xff"

    if (len >= 4 && memcmp(pack, PIC_CMD_FILE_SUCESS, 4) == 0)
    {
        LYY_LOGD(" PIC_CMD_FILE_SUCESS");
        lyy_display_pick(sendPic.sendPack);

    }else if (len >= 4 && memcmp(pack, PIC_CMD_FILE_FAIL, 4) == 0)
    {
        LYY_LOGD(" PIC_CMD_FILE_FAIL");
        lyy_display_setSendPicStatus(LYY_SENDPIC_NORMAL);

    }else if (pack[0] == 0x05)     // 传输成功
    {
        LYY_LOGD(" 0x05");

        // 发送下一包数据
        sendPic.sendPack++;
        lyy_display_pick(sendPic.sendPack);


    }else if(pack[0] == 0x04)      // 传输失败
    {
        LYY_LOGD(" 0x04");
        // 重新发送该包数据
        lyy_display_pick(sendPic.sendPack);
        
    }else if (len >= 4 && memcmp(pack, PIC_CMD_FILE_END, 4) == 0)
    {
        LYY_LOGD(" PIC_CMD_FILE_END");
        // 切换回普通模式
        lyy_display_setSendPicStatus(LYY_SENDPIC_NORMAL);
        if(g_SemSta)
        {
            xSemaphoreGive(hw_httpTranPicUrlSem);  
            g_SemSta = 0x00;
        }
    
    }             

    return 1;
}

/**
 * @description: 外部图片传输接收处理
 * 
 * @param [in]    packet 发送第几包数据
 * 
 * @return {unsigned char } 1成功/0失败
 */
int lyy_display_pick(unsigned int  packet)
{

    unsigned int res        = 1;
    unsigned int readSize   = 0;
    unsigned int offset     = 0;

    // 防卡死
    static unsigned char sendNum        = 0;
    static unsigned char sendNumCount   = 0;

    LYY_LOGD(" send pic sendPic.sendPack =%d",sendPic.sendPack);
    if(sendNum == sendPic.sendPack)
    {
        sendNumCount++;
        if(sendNumCount>=3)
        {
            LYY_LOGD("send pic times out!!!! id=%d", sendPic.sendPack);
            sendNumCount = 0;
            sendNum = 0;
            LYY_LOGD("picSend_sendProcess end");
            memset(&sendPic, 0x00, sizeof(LYY_SENDPIC));
            lyy_display_setSendPicStatus(LYY_SENDPIC_NORMAL);
            lyy_display_sendExitTrans();
            return 0;
        }    
    }
    else
    {
        sendNum = sendPic.sendPack;
        sendNumCount = 0;
    }

    if (packet >= sendPic.totalPack) return 1;
    offset = packet * LYY_SEND_PIC_PACKET_SIZE;
    readSize = sendPic.fileSize - offset;
    if (readSize > LYY_SEND_PIC_PACKET_SIZE) readSize = LYY_SEND_PIC_PACKET_SIZE;

    offset = packet*LYY_SEND_PIC_PACKET_SIZE;

    LYY_LOGD(" send pic offset =%d",offset);

    unsigned char fileBuff[LYY_DISPLAY_BUFF_MAX] = {0x00, };

    // 拼接数据
    memcpy(fileBuff,"\x3a\xa1\xbb\x44\x7f\xff\xfe", 7);         //包头
    fileBuff[7] = 0x00;                                         //无校验
    memcpy(&fileBuff[8],&sendPic.sendPack, 2);                  //第几包数 
    memcpy(&fileBuff[10],&readSize, 2);                         //数据大小


    // 获取要保存的地址文件长度
    sprintf( g_pcPath, 
            picVair.startGetPicType == PIG_WALLET ? "PAY_WALLET_%d" : "PAY_SCAN_%d", 
            picVair.startGetPic);

    res = HAL_vFileReadOffset(  g_pcPath, 
                                (uint8_t *)&fileBuff[12], 
                                readSize,
                                offset);

    LYY_LOGD("===========g_pcPath= [%s] [%d]", g_pcPath, readSize); 
    


    if(res <= 0)
    {
        LYY_LOGD(" picSend_sendProcess read len error");
        return 0;
    }

    LYY_LOGD(" picSend_sendProcess res=%d",res);

    lyy_display_send(fileBuff, readSize + 12);

    return res;

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
 * @description: 显示图片
 * 
 * @param [in]    page 哪一页
 * @param [in]    walletNum 哪一个钱包
 * 
 * @return {unsigned char } 
 * 1成功/
 *  0 参数有误/
 * -1 无该页面图片显示/
 * -2 钱包个数大于最大钱包数/
 * -3 没有对应图片
 */
int lyy_display_showpic(char *page,unsigned char walletNum)
{
    if(page == NULL) return 0;
    if(walletNum == 0) return 0;

    LYY_LOGD("lyy_display_showpic1 %s:%d",page,walletNum);
    LYY_LOGD("lyy_display_showpic2 %s:%d",LYY_PIC_PAY_PAGE_TYPE,g_pxTjcPicFlashInfo.ucWalletNumSum);
    // 选择页面
    if(memcmp(page, LYY_PIC_PACKAGES_PAGE_TYPE, strlen(LYY_PIC_PACKAGES_PAGE_TYPE)) == 0)
    {
        return -1;
    }else if (strstr(page,LYY_PIC_PAY_PAGE_TYPE) != NULL) //
    {

        // 该页是否钱包数是否正确
        if(walletNum > g_pxTjcPicFlashInfo.ucWalletNumSum)
        {
            LYY_LOGD("walletInfo.walletNum =========[%d]",g_pxTjcPicFlashInfo.ucWalletNumSum);
            return -2;
        }

        char file_name[20];

        if (g_pxTjcPicFlashInfo.pucData == NULL) return -3;
        INFOR_PIC_INFO *info = (INFOR_PIC_INFO *)g_pxTjcPicFlashInfo.pucData;
        unsigned int index = (walletNum - 1) * PIC_INFO_MAX;
        if (info[index].filesize == 0 || info[index + 1].filesize == 0) return -3;
        memset(file_name, 0, sizeof(file_name));
        sprintf(file_name, "filename_%d", index);
        lyy_display_setPicPath("wallet", file_name);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        sprintf(file_name, "filename_%d", index + 1);
        lyy_display_setPicPath("wallet_type", file_name);
        vTaskDelay(100 / portTICK_PERIOD_MS);


    }else if (memcmp(page, LYY_PIC_RESULT_PAGE_TYPE, strlen(LYY_PIC_RESULT_PAGE_TYPE)) == 0)
    {
        return -1;
    }else if (memcmp(page, LYY_PIC_WORKING_PAGE_TYPE, strlen(LYY_PIC_WORKING_PAGE_TYPE)) == 0)
    {
        return -1;
    }else if (memcmp(page, LYY_PIC_WARN_PAGE_TYPE, strlen(LYY_PIC_WARN_PAGE_TYPE)) == 0)
    {
        return -1;
    }else
    {
        return -2;
    }
    
    return 1;
}



/**
 * @brief 
 * 图片事件处理
 * @param arg 
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


