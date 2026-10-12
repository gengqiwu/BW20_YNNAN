/*
 * @author: lyz
 * @Date: 2022-10-18 09:58:29
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-07 11:08:12
 * @Description: 
 */
#ifndef __LYY_PIC_H__
#define __LYY_PIC_H__

#include <stdint.h>
#include "cJSON.h"

#include "HAL_System.h"
#include "USER_Config.h"




#define FILE_NAME_HW_PARA_TJC_PIC_INFO          "tjc_pic_info"
#define FILE_NAME_HW_PARA_TJC_PIC_DATA_INFO     "tjc_pic_data_info"
#define FILE_NAME_HW_PARA_TJC_PIC_PACKET_TITLE  "PACKET_TITLE"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_TITLE_0   "PAY_LOGO_0"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_TITLE_1   "PAY_LOGO_1"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_TITLE_2   "PAY_LOGO_2"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_TITLE_3   "PAY_LOGO_3"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_WALLET_0  "PAY_WALLET_0"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_WALLET_1  "PAY_WALLET_1"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_WALLET_2  "PAY_WALLET_2"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_WALLET_3  "PAY_WALLET_3"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_SCAN_0    "PAY_SCAN_0"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_SCAN_1    "PAY_SCAN_1"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_SCAN_2    "PAY_SCAN_2"
#define FILE_NAME_HW_PARA_TJC_PIC_PAY_SCAN_3    "PAY_SCAN_3"


#define FILE_NAME_HW_PARA_TJC_PIC_LOAD      HAL_USER_FLASH_ADDR
#define FILE_NAME_HW_PARA_TJC_PIC_DATA      HAL_USER_FLASH_ADDR + HAL_FLASH_SECTOR_SIZE


//每张图片分配的大小
#define FTLE_NAME_HW_PARA_TJC_PIC_DATA_LEN   (8 * HAL_FLASH_SECTOR_SIZE)

//最多4个渠道，8张图片
#define FTLE_NAME_HW_PARA_TJC_PIC_LEN_MAX    (8)




#define LYY_SCREEN_BRAND        "TJC"          // 品类
#define LYY_SCREEN_DEVICE       "small"        // 小屏

// 旧版本 渠道图片
#define LYY_SCREEN_TYPC_H       "horizontal"    
#define LYY_SCREEN_TYPC_V       "vertical"    

// 新版本
#define LYY_SCREEN_DEVICE_TJC8048x270     "TJC8048x270"      
#define LYY_SCREEN_DEVICE_TJC8048x243     "TJC8048x243"       
#define LYY_SCREEN_DEVICE_TJC4880x243     "TJC4880x243"        
#define LYY_SCREEN_DEVICE_TJC4880x243V1   "TJC4880x243V1"

#define LYY_SCREEN_DEVICE                 "small"                    
#define LYY_SCREEN_DEVICE_4880x243        "small_tjc4880X243"        
#define LYY_SCREEN_DEVICE_4880x243V1      "small_tjc4880X243V1"

#define LYY_SCREEN_TYPC_00              "00"     
#define LYY_SCREEN_TYPC_90              "90"     
#define LYY_SCREEN_TYPC_180             "180"    
#define LYY_SCREEN_TYPC_270             "270"   


#define LYY_PIC_PACKAGES_PAGE_TYPE     "select"
#define LYY_PIC_PAY_PAGE_TYPE          "pay"
#define LYY_PIC_RESULT_PAGE_TYPE       "result"
#define LYY_PIC_WORKING_PAGE_TYPE      "working"
#define LYY_PIC_WARN_PAGE_TYPE         "warn"


typedef enum 
{
    PIG_WALLET,
    PIG_SCAN,
    PIC_INFO_MAX,
}TJC_PIC_TYPE;

typedef enum 
{
    TJC_PIC_NULL,               //  默认状态
    TJC_PIC_GET_URL,            //  获取url
    TJC_PIC_UPDATE_URL,         //  更新
    TJC_PIC_TRAN_INFO,          //  传输图片
}TJC_PIC_DEAL;

typedef enum 
{
    TJC_PIC_STATE_NONE,               //  空闲
    TJC_PIC_STATE_UPDATA,             //  需要更新
    TJC_PIC_STATE_TRAN,               //  需要传输
    TJC_PIC_RESULT_UPDATA_FAIL,       //  更新失败
    TJC_PIC_RESULT_HTTP_FAIL,         //  HTTP获取失败
    TJC_PIC_RESULT_TRAN_FAIL,         //  传输失败
    TJC_PIC_RESULT_TRAN_SUCCESS,      //  已经传输

}TJC_PIC_STATE;



#define CHANNELINFO_MAX 4

// 图片信息
typedef struct 
{
    unsigned int    filesize;
    unsigned char   url[256];
}__attribute__((aligned(1), packed))INFOR_PIC_INFO;



// 图片处理流程变量
typedef struct
{
    unsigned char    process ;
    unsigned char    startGetPic ;
    unsigned char    startGetPicType ;
    unsigned char    picState[CHANNELINFO_MAX*PIC_INFO_MAX];
}__attribute__((aligned(1), packed))INFOR_PIC_VAIR,*PINFOR_PIC_VAIR;


typedef struct {

    uint8_t         ucWalletNumSum;
    uint8_t         *pucData;
}__attribute__((aligned(1), packed))INFOR_PIC_Flash_INFO;

typedef enum
{
    LYY_SENDPIC_NORMAL = 0,
    LYY_SENDPIC_TRANS = 1,
}LYY_DISPLAY_SENDPIC_STATUS;

//发送文件结构体内容
typedef struct
{
    char                fileName[20];       //文件名
    unsigned int        fileSize;           //文件大小
    unsigned int        totalPack;          //总包数
    unsigned int        sendPack;           //发送包号
    unsigned char       state;              //状态
}LYY_SENDPIC,*PLYY_SENDPIC;



#define LYY_DISPLAY_BUFF_MAX        2048
#define LYY_SEND_PIC_PACKET_SIZE    2000   //1000+12



extern void tjc_setProcess(unsigned char newState);
extern void getTjcPicUrl_page(char *page,unsigned char wallet);
extern char lyy_picDown(void *http_para);
extern void lyy_http_get_xiPic(unsigned char type) ;
extern int tjc_recv_pic_url(cJSON* params);
extern int lyy_display_pick(unsigned int  packet);
extern int lyy_display_sendPic(char *  addr, char * fileName, unsigned int  fileSize);
extern unsigned char lyy_display_getSendPicStatus(void);
extern unsigned char lyy_display_setSendPicStatus(unsigned char Status);
extern int lyy_display_sendExitTrans(void);
extern void lyy_display_send(unsigned char *data, unsigned int len);


extern int lyy_display_showpic(char *page,unsigned char walletNum);
extern int lyy_display_show_wallet_icon(uint8_t wallet_id, char *object);
extern void set_tjcScreenType(char * type);
extern int lyy_display_recvPic(unsigned char * pack,unsigned int  len);
extern int lyy_hw_tjc_pic_info_start(void (*pic_result_cb)(INFOR_PIC_VAIR *));
extern void thread_tjcPicDealThread(void *arg);
extern void set_tjcScreen(char * type);
extern void lyy_picInit(void);
extern void lyy_pic_infoAllClean(void);
#endif
