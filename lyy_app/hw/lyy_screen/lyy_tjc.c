/**
 * @file lyy_tjc.c
 * @brief 淘晶驰屏幕页面显示、触摸命令解析及经纸钞主板 D6 的数据透传。
 */
/*
 * @author: e.he
 * @Date: 2022-10-18 09:58:29
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-12-30 11:38:21
 * @Description: 
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "utils.h"
#include "cJSON.h"

#include "APP_NetWork.h"
#include "APP_Param.h"
#include "APP_Event.h"

#include "HAL_Log.h"
#include "HAL_Wifi.h"
#include "HAL_Uart.h"
#include "HAL_System.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "thing_mqtt.h"
#include "thing_event.h"
#include "thing_property.h"
#include "thing_iot.h"
#include "thing_init.h"
#include "lyy_device.h"

#include "lyy_tjc.h"
#include "lyy_pic.h"
#include "lyy_para.h"
#include "lyy_start.h"
#include "lyy_zcj.h"

#include "USER_Config.h"

#define TJC_RECV_SIZE_MAX 10
STORAGE_QUE_DEF tjc_packet_recv_quee[TJC_RECV_SIZE_MAX];
QueueHandle_t tjc_packet_recv_para;


#define NET_RETURN_RESULT(x)  ((x == 0x01) ? "online":"offline")

enum
{
    SCREEN_START_STEP_HOMEPAGE = 0,
    SCREEN_START_STEP_LOGINING,
    SCREEN_START_STEP_WARING_PIC,
    SCREEN_START_STEP_SEND_PIC,
    SCREEN_START_STEP_COUNT_DOWN,
    SCREEN_START_STEP_REGIST,
    SCREEN_START_STEP_UPDATE_NET,
    SCREEN_START_STEP_END,
} SCREEN_START_STEP;

TJC_INFOR_DEF   tjc_infor_t;
/** @brief 屏幕当前 payN 页的钱包数量，其他页面为 0。 */
static uint8_t active_pay_wallet_count;

void lyy_tjc_pay_package_ndj(void);
void lyy_tjc_page_pkgs_ndj(void);
void lyy_tjc_page_pkgs_std(void);
void lyy_tjc_page_change_amy_time(void);

void tjc_touch_return(char *pack, uint16_t len);
void lyy_tjc_touch_time_return(char *pack,uint16_t len);
void tjc_touch_back(char *pack,uint16_t len);
void lyy_tjc_touch_package(char *pack,uint16_t len);
void lyy_tjc_touch_wallet(char *pack,uint16_t len);
void lyy_tjc_time_version(char *pack,uint16_t len);
void lyy_tjc_vlaue_screen_type(char *pack,uint16_t len);
void tjc_touch_regist_next(char *pack,uint16_t len);
void tjc_touch_device_status_display(char *pack,uint16_t len);

const TJC_PACKET_PARSE_DEF tjc_packet_parse_t[] =
    {
        {"back", tjc_touch_back},
        {"package1", lyy_tjc_touch_package},
        {"package2", lyy_tjc_touch_package},
        {"package3", lyy_tjc_touch_package},
        {"package4", lyy_tjc_touch_package},
        {"package5", lyy_tjc_touch_package},
        {"package6", lyy_tjc_touch_package},
        {"return", tjc_touch_return},
        {"wallet0", lyy_tjc_touch_wallet},
        {"wallet1", lyy_tjc_touch_wallet},
        {"wallet2", lyy_tjc_touch_wallet},
        {"wallet3", lyy_tjc_touch_wallet},
        {"screen_ver:", lyy_tjc_time_version},
        {"screen_type:", lyy_tjc_vlaue_screen_type},
        {"time_return", lyy_tjc_touch_time_return},
        {"regist_next", tjc_touch_regist_next},
        {"get_status", tjc_touch_device_status_display},
// todo（先不使用log显示）
#if 0
    {"log_open",  tjc_touch_log_open},
    {"log_close", tjc_touch_log_close},
#endif
};

/**
 * @brief 经纸钞主板 D6 屏幕透传接口发送原始屏幕数据。
 * @param[in] data 屏幕命令或二进制图片数据。
 * @param[in] len 有效字节数。
 * @note 不在此处追加结束符，也不返回主板或屏幕的接收结果。
 */
void lyy_tjc_cmd_send(char *data, uint16_t len)
{
    zcj_packet_screen_dtu_send(data, len);
}

/**
 * @brief 封装控件 txt 设置命令并经主板透传，发送后延时 5 ms。
 * @param[in] objName 控件名称，NULL 时不处理。
 * @param[in] txt 控件文本，NULL 时不处理。
 * @warning 不校验拼接后长度，调用方须保证命令不超出 TJC_BUFF_MAX。
 */
void lyy_tjc_objTxt(char *objName, char *txt)
{

    if(objName == NULL) return;
    if(txt == NULL) return;

    char cmdBuff[TJC_BUFF_MAX] = {0x00,};
    memset(cmdBuff, 0, TJC_BUFF_MAX);

    sprintf(cmdBuff, "%s.txt=\"%s\"\xff\xff\xff", objName, txt);

    //需要更换成
    lyy_tjc_cmd_send(cmdBuff, strlen(cmdBuff));
    vTaskDelay(5 / portTICK_PERIOD_MS);
}

/**
 * @brief 
 * 开关定时器
 * @param objName 
 * @param en 
 */
void lyy_tjc_time_en(char *objName, int en)
{
    if(objName == NULL) return;
    char cmdBuff[TJC_BUFF_MAX] = {0x00,};
    memset(cmdBuff, 0, TJC_BUFF_MAX);
    // LYY_LOGD("change %s to %s", objName, txt);
    sprintf(cmdBuff, "%s.en=%d\xff\xff\xff", objName, en);
    // LYY_LOGD("%s", cmdBuff);
    // LYY_LOGI_hex(cmdBuff, strlen(cmdBuff));
    //需要更换成
    lyy_tjc_cmd_send(cmdBuff, strlen(cmdBuff));
    // lyy_uart_send(UART_NAME_API_TJC, cmdBuff, strlen(cmdBuff));
}
/**
 * @brief 
 * taojingchi串口屏控件txt修改
 * @param objName 对象名称
 * @param txt 对象内容
 */
void lyy_tjc_objbColour(char *objName, uint16_t colour)
{
    if(objName == NULL) return;
    char cmdBuff[TJC_BUFF_MAX] = {0x00,};
    memset(cmdBuff, 0, TJC_BUFF_MAX);
    // LYY_LOGD("change %s to %s", objName, txt);
    sprintf(cmdBuff, "%s.bco=%d\xff\xff\xff", objName, colour);
    // LYY_LOGI_hex(cmdBuff, strlen(cmdBuff));
    lyy_tjc_cmd_send(cmdBuff, strlen(cmdBuff));
}
/**
 * @brief 
 * 隐藏空间
 * @param objName 
 * @param val 0:隐藏 1:显示
 */
void lyy_tjc_objHide(char *objName, uint8_t val)
{
    if(objName == NULL) return;
    char cmdBuff[TJC_BUFF_MAX] = {0x00,};
    memset(cmdBuff, 0, TJC_BUFF_MAX);
    // LYY_LOGD("change %s to %s", objName, txt);
    sprintf(cmdBuff, "vis %s,%d\xff\xff\xff", objName, val);
    // LYY_LOGI_hex(cmdBuff, strlen(cmdBuff));
    lyy_tjc_cmd_send(cmdBuff, strlen(cmdBuff));
}
/**
 * @brief 
 * 切换页面
 * @param page 页面名称
 */
void lyy_tjc_switch_page(char *page)
{
    active_pay_wallet_count = 0;
    char cmdBuff[TJC_BUFF_MAX] = {0x00,};
    memset(cmdBuff, 0, TJC_BUFF_MAX);
    sprintf(cmdBuff, "page %s\xff\xff\xff", page);
    LYY_LOGD("lyy_tjc_switch_page : %s",cmdBuff);
    lyy_tjc_cmd_send(cmdBuff, strlen(cmdBuff));
 
    vTaskDelay(5 / portTICK_PERIOD_MS);  
}
/**
 * @brief 
 * 获取变量值
 * @param obj 
 */
void lyy_tjc_get_value_txt(char *obj)
{
    char cmdBuff[TJC_BUFF_MAX] = {0x00,};
    memset(cmdBuff, 0, TJC_BUFF_MAX);
    sprintf(cmdBuff, "get %s.txt\xff\xff\xff", obj);
    lyy_tjc_cmd_send(cmdBuff, strlen(cmdBuff));
    vTaskDelay(5 / portTICK_PERIOD_MS);   
}
/**
 * @brief 
 * 显示套餐
 * @param number 
 * @param price 
 * @param unit 
 */
void lyy_tjc_showPrice(uint8_t number,float price,char *unit)
{
    char txt[128] = {0};
    char temp[128] = {0};
    sprintf(txt, "lane%d", number);
    sprintf(temp, "Cabinet %d", number);
    lyy_tjc_objTxt(txt, temp);
    sprintf(txt, "price%d", number);
    sprintf(temp, "%.2f %s", price,unit);
    lyy_tjc_objTxt(txt, temp);
}



/**
 * @brief 
 * 显示单个套餐--底层接口
 * @param txt1 
 * @param txt2 
 * @param txt3 
 * @param num  套餐编号
 */
void lyy_tjc_package(uint8_t num,char *txt1,char *txt2,char *txt3)
{
    char temp[32] = {0};
    sprintf(temp, "pkg%d_txt1", num);
    lyy_tjc_objTxt(temp, txt1);
    vTaskDelay(2 / portTICK_PERIOD_MS);  
    sprintf(temp, "pkg%d_txt2", num);
    lyy_tjc_objTxt(temp, txt2);
    vTaskDelay(2 / portTICK_PERIOD_MS);  
    sprintf(temp, "pkg%d_txt3", num);
    lyy_tjc_objTxt(temp, txt3);
    vTaskDelay(2 / portTICK_PERIOD_MS);  
}

/**
 * @brief 
 * 显示单个套餐--标准接口
 * @param num 
 * @param money 
 * @param price 
 * @param money_unit 
 * @param price_unit 
 */
void lyy_tjc_package_std(uint8_t num,char *money,char *price)
{
    char temp[32] = {0};
    sprintf(temp, "NO.%d", num);
    lyy_tjc_package(num,temp,money,price);    
}


/**
 * @brief 
 * 隐藏套餐
 * @param id 
 */
void lyy_tjc_clean_package_std(uint8_t id)
{
    // LYY_LOGD("lyy_tjc_ndj_clean_price : %d",id);
    if(id>6||id == 0) return ;
    char temp[20] = {0};
    sprintf(temp, "bg%d", id);
    lyy_tjc_objHide(temp, 0);
    vTaskDelay(5 / portTICK_PERIOD_MS);  
    memset(temp,0,sizeof(temp));
    sprintf(temp, "m%d", id);
    lyy_tjc_objHide(temp, 0);
}

/**
 * @brief 
 * 显示套餐
 * @param id 
 */
void lyy_tjc_vis_package_std(uint8_t id)
{
    // LYY_LOGD("lyy_tjc_ndj_clean_price : %d",id);
    if(id>6||id == 0) return ;
    char temp[20] = {0};
    sprintf(temp, "bg%d", id);
    lyy_tjc_objHide(temp, 1);
    vTaskDelay(5 / portTICK_PERIOD_MS);  
    memset(temp,0,sizeof(temp));
    sprintf(temp, "m%d", id);
    lyy_tjc_objHide(temp, 1);
}
/**
 * @brief 
 * 显示套餐页
 */
void lyy_tjc_package_page(void)
{
    PAY_PACKAGES_DEF *pkgs = get_pay_packages();
    char price[128]={0};
    char money[128]={0};
    uint8_t i = 0;
    lyy_tjc_switch_page("package");//切换页面
    vTaskDelay(10 / portTICK_PERIOD_MS);  
    lyy_tjc_device_infor();
    if(pkgs->sum<6)//隐藏不需要的套餐
    {
        for(i=pkgs->sum;i<6;i++)
        {
            lyy_tjc_clean_package_std(i+1); //隐藏
            vTaskDelay(10 / portTICK_PERIOD_MS);   
        }
    }
    for(i=0;i<pkgs->sum;i++) //
    {
        lyy_tjc_vis_package_std(i+1);
        sprintf(money,"%s %s",pkgs->pkg[i].display_price,pkgs->currency_name);
        sprintf(price,"%s %s",pkgs->pkg[i].display_value,pkgs->pkg[i].unit);
        lyy_tjc_package_std(i+1,money,price); 
        vTaskDelay(1 / portTICK_PERIOD_MS);   
    }
}
/**
 * @brief 
 * 
 */
void lyy_tjc_device_infor(void)
{
    // char str[200] = {0};
    // sprintf(str, "phone%s", tjc_infor_t.telephone);

    lyy_tjc_objTxt("net_status", NET_RETURN_RESULT(lyy_mqtt_get_status())); 
    vTaskDelay(2 / portTICK_PERIOD_MS);  
    lyy_tjc_objTxt("devid", APP_pcGetParamDevId());  
    vTaskDelay(2 / portTICK_PERIOD_MS);  
    lyy_tjc_objTxt("ver", CFG_APP_VERSION); 
    vTaskDelay(2 / portTICK_PERIOD_MS);
}
/**
 * @brief 
 * 回调信息
 * @param param 
 */
void lyy_pic_get_cb(INFOR_PIC_VAIR *param)
{
    
    PAY_PARAM_DEF *pay= get_pay_param();
    for(uint8_t i = 0;i<pay->chnl_sum * PIC_INFO_MAX;i++) //每个钱包都有pic
    {
        LYY_LOGD("param->picState[%d] = [%d]",i,param->picState[i]);
        tjc_infor_t.img_result = param->picState[i];
    }
}
/**
 * @brief 
 * 开启图片获取
 * < 0 失败 
 */
int lyy_start_pic_get(void)
{
    return lyy_hw_tjc_pic_info_start(lyy_pic_get_cb);
}
#if 0
/**
 * @brief 
 * 显示支付页面
 * @param number_wallet 
 * @param wallet_sum 
 * @param url 
 * @param money 
 */
void lyy_tjc_pay_page(PTJC_PAY_PAGE_DEF pay)
{
    char money_unit[128] = {0};
    char page_name[20] = {0};
    sprintf(money_unit,"%s %s",pay->money,get_money_unit());
    sprintf(page_name,"pay%d",pay->wallet_sum);
    lyy_tjc_switch_page(page_name);
    int ret = lyy_display_showpic("pay",pay->wallet_num+1);
    LYY_LOGD("lyy_display_showpic :%d",ret);
    vTaskDelay(5 / portTICK_PERIOD_MS);  
    LYY_LOGD("lyy_tjc_pay_page pay_url:%s",pay->url);
    lyy_tjc_objTxt("pay_url", pay->url); 
    vTaskDelay(5 / portTICK_PERIOD_MS);  
    lyy_tjc_objTxt("money", money_unit);
    vTaskDelay(5 / portTICK_PERIOD_MS);  
    lyy_tjc_device_infor();
}
#endif
/**
 * @brief 在需要时打开与渠道数量匹配的支付页。
 * @param[in] wallet_sum 支付页的钱包数量，对应 payN 中的 N。
 * @note 同页切换钱包时保留屏幕页内的控件状态。其他页面切换会清除记录。
 */
static void lyy_tjc_ensure_pay_page(uint8_t wallet_sum)
{
    if (active_pay_wallet_count != 0 && active_pay_wallet_count == wallet_sum) return;

    char page_name[20];
    snprintf(page_name, sizeof(page_name), "pay%u", (unsigned int)wallet_sum);
    lyy_tjc_switch_page(page_name);
    active_pay_wallet_count = wallet_sum;
}

/**
 * @brief 刷新支付页的渠道图片、金额及非空二维码链接。
 * @param[in] pay 有效支付页参数，金额与 get_money_unit() 拼接显示。
 * @note 云端回包先于等待页渲染时，也会先打开正确的支付页。
 */
void lyy_tjc_pay_url_money(PTJC_PAY_PAGE_DEF pay)
{
    char money_unit[128] = {0};
    sprintf(money_unit, "%s %s", pay->money, get_money_unit());
    lyy_tjc_ensure_pay_page(pay->wallet_sum);
    lyy_display_showpic("pay",pay->wallet_num+1);
    vTaskDelay(5 / portTICK_PERIOD_MS);  
    lyy_tjc_objTxt("money", money_unit);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    LYY_LOGD("lyy_tjc_pay_url_money money:%s",money_unit);    
    if(strlen(pay->url) != 0)
    {
        LYY_LOGD("lyy_tjc_pay_url_money pay_url:%s",pay->url);
        lyy_tjc_objTxt("pay_url", pay->url); 
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
    
    lyy_tjc_device_infor();
}
/**
 * @brief 在支付页更新当前钱包，并清空上一钱包显示的二维码和金额。
 * @param[in] wallet_sum 本页钱包总数。
 * @param[in] wallet_num 当前钱包的零基下标。
 */
void lyy_tjc_change_pay_page(uint8_t wallet_sum,uint8_t wallet_num)
{
    lyy_tjc_ensure_pay_page(wallet_sum);
    lyy_tjc_objTxt("pay_url", "");
    vTaskDelay(2 / portTICK_PERIOD_MS);    
    lyy_tjc_objTxt("money", " ");
    vTaskDelay(2 / portTICK_PERIOD_MS);    
    lyy_tjc_objTxt("time", " ");
    vTaskDelay(2 / portTICK_PERIOD_MS);    
    int ret = lyy_display_showpic("pay",wallet_num+1);
    LYY_LOGD("lyy_display_showpic :%d",ret);
    vTaskDelay(2 / portTICK_PERIOD_MS);  
    lyy_tjc_device_infor();
}
#if 0
/**
 * @brief 
 * 
 * @param wallet_num 
 * @param wallet_sum 
 * @param url 
 * @param money 
 */
void lyy_tjc_pay_page_set(uint8_t wallet_num,uint8_t wallet_sum,char *url,char *money)
{
    memset(tjc_infor_t.pay.url,0,sizeof(tjc_infor_t.pay.url));
    memset(tjc_infor_t.pay.money,0,sizeof(tjc_infor_t.pay.money));
    strcpy(tjc_infor_t.pay.url,url);
    strcpy(tjc_infor_t.pay.money,money);
    tjc_infor_t.pay.wallet_num = wallet_num;
    tjc_infor_t.pay.wallet_sum = wallet_sum;
    tjc_infor_t.pay.time   = 40;  //倒计时固定40秒
    lyy_tjc_set_status(TJC_SHOW_PAY);
}
#endif

/**
 * @brief 
 * 显示支付页面
 * @param wallet_num 
 * @param wallet_sum 
 */
void lyy_tjc_pay_page_set(uint8_t wallet_num,uint8_t wallet_sum)
{
    tjc_infor_t.pay.wallet_num = wallet_num;
    tjc_infor_t.pay.wallet_sum = wallet_sum;
    tjc_infor_t.pay.time   = 40;  //倒计时固定40秒
    lyy_tjc_set_status(TJC_SHOW_PAY_PAGE);
    vTaskDelay(20 / portTICK_PERIOD_MS);  
}
/**
 * @brief 
 * 显示二维码
 * @param url 
 * @param money 
 */
void lyy_tjc_pay_url_show(char *url,char *money)
{
    memset(tjc_infor_t.pay.url,0,sizeof(tjc_infor_t.pay.url));
    memset(tjc_infor_t.pay.money,0,sizeof(tjc_infor_t.pay.money));
    strcpy(tjc_infor_t.pay.url,url);
    strcpy(tjc_infor_t.pay.money,money);
    tjc_infor_t.pay.time   = 40;  //倒计时固定40秒
    lyy_tjc_set_status(TJC_SHOW_PAY_URL);
}

// /**
//  * @brief 
//  * 显示二维码
//  * @param url 
//  * @param money 
//  */
// void lyy_tjc_pay_url_show(char *url,char *money)
// {
//     memset(tjc_infor_t.pay.url,0,sizeof(tjc_infor_t.pay.url));
//     memset(tjc_infor_t.pay.money,0,sizeof(tjc_infor_t.pay.money));
//     strcpy(tjc_infor_t.pay.url,url);
//     strcpy(tjc_infor_t.pay.money,money);
//     tjc_infor_t.pay.time   = 40;  //倒计时固定40秒
//     lyy_tjc_set_status(TJC_SHOW_PAY_URL);
// }
/**
 * @brief 根据图片套餐编译选项显示套餐页，非图片分支统一显示通用套餐。
 */
void lyy_tjc_packages_show(void)
{
    #ifdef _PIC_PKG_
    switch (get_dev_type_code())
    {

        case DEVICE_TYPE_UART_XYJ: lyy_tjc_package_page();break;
        default:lyy_tjc_package_page();break;
    }
    #else
    switch (get_dev_type_code())
    {
        default: lyy_tjc_page_pkgs_std(); break;
    }
    #endif
}
/**
 * @brief 
 * 显示套餐
 * @param money 
 * @param value 
 */
void lyy_tjc_pkg_std(char *money,char *value,uint8_t pkg_num)
{
    char name[20]={0};
    char txt[128] = {0};
    sprintf(name,"pkg%d",pkg_num);
    sprintf(txt,"%s\\r%s",money,value);
    LYY_LOGD("%s:%s",name,txt);
    lyy_tjc_objTxt(name, txt);    
}
/**
 * @brief 
 * 隐藏套餐
 * @param id 
 */
void lyy_tjc_clean_pkg_std(uint8_t id)
{
    // LYY_LOGD("lyy_tjc_ndj_clean_price : %d",id);
    if(id>6||id == 0) return ;
    char temp[20] = {0};
    sprintf(temp, "pkg%d", id);
    lyy_tjc_objHide(temp, 0);
    vTaskDelay(5 / portTICK_PERIOD_MS);  
}
/**
 * @brief 
 * 显示套餐
 * @param id 
 */
void lyy_tjc_vis_pkg_std(uint8_t id)
{
    // LYY_LOGD("lyy_tjc_ndj_clean_price : %d",id);
    if(id>6||id == 0) return ;
    char temp[20] = {0};
    sprintf(temp, "pkg%d", id);
    lyy_tjc_objHide(temp, 1);
    vTaskDelay(5 / portTICK_PERIOD_MS);  
}

/**
 * @brief 切换到 pkg_std2 页面并按缓存刷新套餐控件。
 * @note 从套餐缓存的零基下标读取，控件编号从 1 开始；多余控件隐藏，显示金额、价值及单位。
 */
void lyy_tjc_page_pkgs_std(void)
{
    PAY_PACKAGES_DEF *pkgs = get_pay_packages();
    char price[128]={0};
    char money[128]={0};
    uint8_t i = 0;
    lyy_tjc_switch_page("pkg_std2");
    vTaskDelay(10 / portTICK_PERIOD_MS);  
    lyy_tjc_device_infor();
    if(pkgs->sum<6)//隐藏不需要的套餐
    {
        for(i=pkgs->sum+1;i<=6;i++)  //套餐ID 从1 存储也是从1开始
        {
            lyy_tjc_clean_pkg_std(i); //隐藏
            vTaskDelay(10 / portTICK_PERIOD_MS);   
        }
    }
    for(i=0;i<pkgs->sum;i++) //
    {
        lyy_tjc_vis_pkg_std(i+1);
        sprintf(money,"%s %s",pkgs->pkg[i].display_price,pkgs->currency_name);
        sprintf(price,"%s %s",pkgs->pkg[i].display_value,pkgs->pkg[i].unit);
        lyy_tjc_pkg_std(money,price,i+1);
        vTaskDelay(10 / portTICK_PERIOD_MS);   
    } 
}

/**
 * @brief 保留扭蛋机套餐控件接口，本固件不执行显示。
 * @param[in] name 未使用。
 * @param[in] money 未使用。
 * @param[in] value 未使用。
 * @param[in] pkg_num 未使用。
 */
void lyy_tjc_pkg_ndj(char *name,char *money,char *value,uint8_t pkg_num)
{

    (void)name; (void)money; (void)value; (void)pkg_num;

}

/**
 * @brief 保留扭蛋机套餐页入口，本固件不执行显示。
 */
void lyy_tjc_page_pkgs_ndj(void)
{
    /* Not supported in the dedicated ZCJ branch. */
}

/**
 * @brief 
 * 字符过长则换行
 * @param src 
 * @param obj 
 * @param len 
 * @return int 行数
 */
int tjc_txt_wrap(char *src,char *obj,uint8_t len,uint8_t mem_len)
{
    char temp[128] ={0};
    char temp2[256] ={0};
    uint8_t string_len = 0;
    string_len = strlen(src);
    LYY_LOGD("string_len :%d,sizeof(obj) :%d",string_len,mem_len);
    if(string_len >= mem_len) return -1;
    if(string_len <= len)
    {
        sprintf(obj," %s",src);
        LYY_LOGD("obj :%s",obj);
        return 1;
    } 
    else if (string_len <= 2*len)
    {
        memcpy(temp,src,len);
        sprintf(temp2," %s\\r",temp);
        memcpy(obj,temp2,strlen(temp2));
        memset(temp2,0,sizeof(temp2));
        memset(temp,0,sizeof(temp));
        memcpy(temp,src+len,len);
        sprintf(temp2," %s",temp);
        strcat(obj,temp2);
        LYY_LOGD("obj :%s",obj);
        return 2;    
    }   
    return -1;
}

/**
 * @brief 
 * 显示价格
 * @param id    套餐ID从1开始 
 * @param price 价格
 * @param name  礼品名称
 */
void lyy_tjc_ndj_price(uint8_t id,char *price,char *name)
{
    char txt[20] = {0};
    char temp[31] = {0};
    // LYY_LOGD("lyy_tjc_ndj_price : %d",id);
    if(id>4 || id == 0) return ;
    sprintf(txt, "package%d", id);
    sprintf(temp, " NO.%d", id);
    lyy_tjc_objTxt(txt, temp);
    sprintf(txt, "price%d", id);
    lyy_tjc_objTxt(txt, price);
    sprintf(txt, "name%d", id);
    memset(temp,0,sizeof(temp));
    // tjc_txt_wrap(name,temp,14,sizeof(temp));
    if(strlen(name)>30) 
    {
        memcpy(temp,name,30);
        temp[30] = '\0';
    }
    else 
    {
        strcpy(temp,name);
    }
    lyy_tjc_objTxt(txt, temp);
}
/**
 * @brief 
 * 隐藏套餐
 * @param id 
 */
void lyy_tjc_ndj_clean_price(uint8_t id)
{
    // LYY_LOGD("lyy_tjc_ndj_clean_price : %d",id);
    if(id>4||id == 0) return ;
    char temp[20] = {0};
    sprintf(temp, "bg%d", id);
    lyy_tjc_objHide(temp, 0);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    memset(temp,0,sizeof(temp));
    sprintf(temp, "m%d", id);
    lyy_tjc_objHide(temp, 0);
}
/**
 * @brief 
 * 显示套餐
 * @param id 
 */
void lyy_tjc_ndj_vis_price(uint8_t id)
{
    char temp[20] = {0};
    if(id>4||id == 0) return ;
    // LYY_LOGD("lyy_tjc_ndj_vis_price : %d",id);
    sprintf(temp, "bg%d", id);
    lyy_tjc_objHide(temp, 1);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    memset(temp,0,sizeof(temp));
    sprintf(temp, "m%d", id);
    lyy_tjc_objHide(temp, 1);
}
/**
 * @brief 
 * 显示套餐
 * @param id 
 * @param unit 
 * @param price 
 * @param name 
 */
void lyy_tjc_ndj_priceShow(uint8_t id,char *unit,float price,char *name)
{
    char temp[128] = {0};
    lyy_tjc_ndj_vis_price(id);
    // LYY_LOGD("lyy_tjc_ndj_priceShow :%d",id);
    sprintf(temp, "%.2f %s", price,unit);
    lyy_tjc_ndj_price(id,temp,name);
}
/**
 * @brief 保留扭蛋机支付套餐显示入口，本固件不执行操作。
 */
void lyy_tjc_pay_package_ndj(void)
{
    /* NDJ is not supported by this firmware. */
}



/**
 * @brief 
 * 显示成功页
 * @param time 
 * @param next_status 
 */
void lyy_tjc_pay_result(uint16_t time,uint8_t next_status)
{
    tjc_infor_t.success.time = time;
    tjc_infor_t.success.next_status = next_status;
    lyy_tjc_set_status(TJC_SHOW_PAY_RESULT);    
}
/**
 * @brief 请求显示支付结果 10 秒后返回套餐状态。
 * @note 仅修改显示状态，不判断支付或出钞是否成功。
 */
void lyy_tjc_page_pay_reuslt(void)
{
    lyy_tjc_pay_result(10, TJC_SHOW_PACKAGES);
}
/**
 * @brief 切换到 result 页面并刷新设备信息。
 */
void lyy_tjc_page_obj_pay_result(void)
{
    lyy_tjc_switch_page("result");
    lyy_tjc_device_infor();
}

/**
 * @brief MQTT 连接状态变化时更新 net_status 文本。
 * @note 使用静态缓存抑制相同状态的重复显示，初始离线状态不主动刷新。
 */
void lyy_tjc_net_status(void)
{
    static uint8_t status = 0;
    if(lyy_mqtt_get_status() == status)  return;
    status = lyy_mqtt_get_status();
    lyy_tjc_objTxt("net_status", NET_RETURN_RESULT(lyy_mqtt_get_status()));
}
/**
 * @brief 
 * 显示套餐
 */
void lyy_tjc_page_package(void)
{
   lyy_tjc_set_status(TJC_SHOW_PACKAGES); 
}

/**
 * @brief 
 * 获取目前showState
 * @param arg 
 */
uint8_t lyy_tjc_get_status(void)
{
    return tjc_infor_t.status;
}
/**
 * @brief 
 * 
 * @return uint8_t 
 */
void lyy_tjc_set_status(uint8_t status)
{
    tjc_infor_t.status = status;
}
/**
 * @brief 
 * 
 * @param dev_name 
 * @param dev_ver 
 * @param body 
 */
void lyy_tjc_page_obj_homepage(char *dev_name,char *dev_ver,char *body)
{

    (void)dev_ver;

    vTaskDelay(5 / portTICK_PERIOD_MS); 
    lyy_tjc_device_infor();
    vTaskDelay(5 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("dev_name",dev_name);
    vTaskDelay(5 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("tjc_ver", tjc_infor_t.tjc_ver);
    vTaskDelay(5 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("context", body);    
}




/**
 * @brief 
 * 获取屏幕类型
 */
void lyy_tjc_screen_type_get(void)
{
    lyy_tjc_get_value_txt("screen_type");
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
    lyy_tjc_get_value_txt("screen_type");
    vTaskDelay(1000 / portTICK_PERIOD_MS); 
}

/**
 * @brief 
 * 
 */
void lyy_tjc_page_homepage(void)
{

    char body[512] = {0};
    LYY_LOGD("lyy_tjc_page_homepage");

    sprintf(body,"net_type : WIFI csq : %d\\rlanguage : English\\rmodel : RTL8711", HAL_lGetWifiRssiToCsq());

    lyy_tjc_page_obj_homepage("Gashapon machine", CFG_APP_VERSION, body);
    
}
/**
 * @brief 
 * 
 * @param status 
 * @return char* 
 */
char *tjc_get_login_status(void)
{
    switch(lyy_mqtt_get_step())
    {
        case MQTT_STATE_STANDBY:return "standy";break;
        case MQTT_STATE_INIT:return "initialization";break;
        case MQTT_STATE_CONNECT:return "connect";break;
        case MQTT_STATE_SUBSCRIBE:return "subscribe";break;
        case MQTT_STATE_SUCCESS:return "success";break;
        default : return "other";break;
    }
}
/**
 * @brief 
 * 
 * @param status 
 * @return char* 
 */
char *tjc_get_net_status(void)
{
    switch(APP_pxGetClientSta())
    {
        case CLIENT_DIS_CON : return "close";
        case CLIENT_CONNING : return "open socket";
        case CLIENT_CONED   : return "open success";
        default : return "other";
    }
}
/**
 * @brief 
 * 显示设备版本信息
 */
void lyy_tjc_obj_device_infor(void)
{
    vTaskDelay(5 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("dev_name",APP_pcGetDevType());
    vTaskDelay(5 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("tjc_ver",tjc_infor_t.tjc_ver);
}
/**
 * @brief 
 * 
 */
void lyy_tjc_page_logining(void)
{

 
    char body[512] = {0};
    char temp[5][50] = {0};

    memset(temp,0,sizeof(temp));
    sprintf(temp[0],"net_type: WIFI      csq:%d",  HAL_lGetWifiRssiToCsq());
    sprintf(temp[1],"language: English ; brt:%ld ", APP_ulGetParamBaudRate());
    sprintf(temp[2],"hardware: %s",                 CFG_HARD_VERSION);
    sprintf(temp[3],"net_status:    %s",            tjc_get_net_status());
    sprintf(temp[4],"login_status:  %s",            tjc_get_login_status());
    

    sprintf(body,"%s\\r%s\\r%s\\r%s\\r%s",temp[0],temp[1],temp[2],temp[3],temp[4]);
    lyy_tjc_objTxt("context", body);   
    vTaskDelay(5 / portTICK_PERIOD_MS); 
    lyy_tjc_device_infor();
    lyy_tjc_obj_device_infor();

}



/**
 * @brief 保存倒计时参数并切换到倒计时显示状态。
 * @param[in] time 倒计时时间。
 * @param[in] next_status 倒计时结束后目标显示状态。
 * @note time 为 0 时延时 1000 ms 后使用 waring.next_status，而非本次 next_status。
 */
void lyy_tjc_count_down(uint16_t time, uint8_t next_status)
{
    tjc_infor_t.count_down.time = time;
    tjc_infor_t.count_down.next_status = next_status;
    if(tjc_infor_t.count_down.time >0) tjc_infor_t.status = TJC_SHOW_COUNT_DOWN;
    else 
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS); 
        tjc_infor_t.status = tjc_infor_t.waring.next_status;   
    }
}

/**
 * @brief 
 * 正常提示
 * 3秒，下一个状态套餐页面
 * @param context 
 */
void lyy_tjc_page_waring_normal(char *context)
{
    lyy_tjc_page_waring(context,3,TJC_SHOW_PACKAGES);    
}
/**
 * @brief 
 * 故障信息提示
 * @param context 
 * @param code 
 */
void lyy_tjc_page_fault_set(uint16_t code)
{
    char temp[128] = {0};
    tjc_infor_t.fault_code = code;
    sprintf(temp,"waring[%d] : %s",code,get_fault_msg(code));
    lyy_tjc_page_waring(temp,4,TJC_SHOW_PACKAGES);    
}
/**
 * @brief 
 * 报警提示
 * @param context 
 * @param time 
 * @param next_status 
 */
void lyy_tjc_page_waring(char *context,uint16_t time,uint8_t next_status)
{
    memset(tjc_infor_t.waring.context,0,sizeof(tjc_infor_t.waring.context));
    strcpy(tjc_infor_t.waring.context,context); 
    tjc_infor_t.waring.time   =  time;
    tjc_infor_t.waring.next_status = next_status;
    tjc_infor_t.status = TJC_SHOW_WARING;
}
/**
 * @brief 
 * 显示提示信息
 * @param time 
 * @param context 
 */
void lyy_tjc_page_obj_waring(char *context,uint16_t time)
{
    char temp[10]={0};
    lyy_tjc_switch_page("waring");        //切换到首页
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    sprintf(temp,"%dS",time);
    lyy_tjc_objTxt("time", temp);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("body", context);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_device_infor();
}

/**
 * @brief 
 * 出币显示 中间
 * @param inc 
 * @param time 
 */
void lyy_tjc_dbj_coins_out(uint16_t sum,uint16_t inc,uint16_t time)
{
    char temp[10] = {0};
    char coins[20] = {0};
    sprintf(coins,"%d / %d",sum,inc);
    lyy_tjc_objTxt("body", coins);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    sprintf(temp,"%dS",time);
    lyy_tjc_objTxt("time", temp);
    vTaskDelay(2 / portTICK_PERIOD_MS);       
}
/**
 * @brief 
 * 开启出币
 * @param sum 
 * @param inc 
 */
void lyy_tjc_dbj_coins_set(uint16_t sum,uint16_t inc)
{
    tjc_infor_t.dbj_coins.inc = inc;
    tjc_infor_t.dbj_coins.sum = sum;
    tjc_infor_t.status = TJC_SHOW_DBJ_COINS;
}
/**
 * @brief 
 * 触摸返回套餐首页
 * @param pack 
 * @param len 
 */
void tjc_touch_return(char *pack,uint16_t len)
{

    (void)pack;
    (void)len;

    lyy_tjc_set_status(TJC_SHOW_PACKAGES);
}

/**
 * @brief 
 * 
 * @param pack 
 * @param len 
 */
void lyy_tjc_touch_time_return(char *pack,uint16_t len)
{

    (void)pack;
    (void)len;

    lyy_tjc_set_status(TJC_SHOW_PACKAGES);
}
/**
 * @brief 
 * 返回上一页
 * @param pack 
 * @param len 
 */
void tjc_touch_back(char *pack,uint16_t len)
{

    (void)pack;
    (void)len;

    lyy_tjc_set_status(TJC_SHOW_PACKAGES);
}

/**
 * @brief
 *
 * @param pack
 * @param len
 */
void tjc_touch_regist_next(char *pack, uint16_t len)
{
    (void)pack;
    (void)len;
}

/**
 * @brief 
 * 机器状态显示
 * 包括倒计时，出币，故障信息等
 */
void tjc_touch_device_status_display(char *pack,uint16_t len)
{

    (void)pack;
    (void)len;

    if(amy_time_redisplay()==0x00) //不需要显示倒计时
    {
        lyy_tjc_page_fault_set(tjc_infor_t.fault_code);    
    }
}

/**
 * @brief 
 * 获取状态主要用于显示倒计时，出币，故障信息等
 * @param pack 
 * @param len 
 */
void tjc_touch_get_status(char *pack,uint16_t len)
{
    (void)pack;
    (void)len;
}
/**
 * 
 * @brief 
 * 判断参数是否正确
 * @param package_id 
 * @param wallet_id 
 */
int lyy_tjc_pay_id_isok(uint8_t package_id ,uint8_t wallet_id)
{
    PAY_PARAM_DEF *para = get_pay_param();
    char temp[64]={0};
    if(package_id >= NDJ_LANE_MAX) 
    {
        sprintf(temp,"device parameter: ndj[%d] package is error ",package_id);
        lyy_tjc_page_waring_normal(temp);
        return -1;
    }
    
    if(strlen(para->channel[wallet_id].code) == 0)
    {
        sprintf(temp,"pay channel: channel[%d] code is null",wallet_id);
        lyy_tjc_page_waring_normal(temp);
        return -2; 
    }
    else if(strlen(para->channel[wallet_id].channel_number) == 0)
    {
        sprintf(temp,"pay channel: channel[%d] number is null",wallet_id);
        lyy_tjc_page_waring_normal(temp);
        return -3; 
    } 
    return 0x01;   
}

/**
 * @brief 保留扭蛋机二维码请求入口，本固件不发送请求。
 * @param[in] id 未使用。
 * @param[in] wallet_id 未使用。
 */
void lyy_tjc_get_pay_url_ndj(uint8_t id,uint8_t wallet_id)
{
    (void)id; (void)wallet_id;
}
/**
 * @brief 校验套餐及钱包后请求通用动态支付二维码。
 * @param[in] id 套餐缓存的零基下标。
 * @param[in] wallet_id 支付渠道的零基下标。
 * @note 校验失败显示提示并返回；此入口不再发送积分抵扣请求。
 */
void lyy_tjc_get_pay_url_std(uint8_t id,uint8_t wallet_id)
{
    PAY_PARAM_DEF *para = get_pay_param();
    char temp[64] = {0};
    LYY_LOGD("lyy_tjc_get_pay_url_std [package_id] :%d",id);
    LYY_LOGD("lyy_tjc_get_pay_url_std [wallet_id] :%d",wallet_id);   
    int ret = lyy_tjc_pay_id_isok(id, wallet_id);
    if(ret < 0 ) return ;
    if(strlen(para->pkgs.pkg[id].id) == 0) //没有商品ID
    {
        sprintf(temp,"pay packages[%d]: sku_id is null ",id);
        lyy_tjc_page_waring_normal(temp);
        LYY_LOGD("sku_id is null");
        //提示故障
        return ;
    }
    if(id > para->pkgs.sum) 
    {
        lyy_tjc_page_waring_normal("pay packages: pakcage_id is error");
        LYY_LOGD("pakcage_id is error");
        //显示故障
        return ;
    }

    event_post_query_dynamic_url_std(para->pkgs.pkg[id].id,
                                   para->channel[wallet_id].code,
                                   para->channel[wallet_id].channel_number);
 
    LYY_LOGD("region :%s",para->channel[wallet_id].region);
    LYY_LOGD("channelCode :%s",para->channel[wallet_id].channel_code);
    LYY_LOGD("wallet_code :%s",para->channel[wallet_id].code);
}
/**
 * @brief 
 * 获取支付二维码
 * @param id 
 * @param wallet_id 
 */
void lyy_tjc_get_pay_qrcode(uint8_t id,uint8_t wallet_id)
{
    pay_url_wallet_number_set(wallet_id);//默认使用第一个钱包
    pay_url_package_id_set(id);
    PAY_PARAM_DEF *param = get_pay_param();
    lyy_tjc_pay_page_set(wallet_id,param->chnl_sum);
    if(get_dev_type_code() == DEVICE_TYPE_UART_NDJ) 
        lyy_tjc_get_pay_url_ndj(id,wallet_id);
    else
        lyy_tjc_get_pay_url_std(id,wallet_id);
}
/**
 * @brief 
 * 选择套餐
 * @param id  //套餐ID 扭蛋机一个套餐一个仓 ID 也相当于仓号
 * @param wallet_number //钱包序号 从0开始
 */
void lyy_tjc_packages_choose(uint8_t id,uint8_t wallet_number)
{
    LYY_LOGD("lyy_tjc_packages_choose package_id: %d ,wallet_id :%d",id,wallet_number);
    lyy_tjc_get_pay_qrcode(id,wallet_number); //显示等待支付二维码界面
}
/**
 * @brief 将套餐按钮名称转换为缓存下标。
 * @param[in] package 有效的零结尾按钮字符串。
 * @return package1～package8 对应 0～7，未匹配返回 8。
 */
int get_package_id(char *package)
{
    if(strstr(package,"package1")!=NULL) return 0;   //存储从0开始
    else if(strstr(package,"package2")!=NULL) return 1;
    else if(strstr(package,"package3")!=NULL) return 2;
    else if(strstr(package,"package4")!=NULL) return 3;
    else if(strstr(package,"package5")!=NULL) return 4;
    else if(strstr(package,"package6")!=NULL) return 5;
    else if(strstr(package,"package7")!=NULL) return 6;
    else if(strstr(package,"package8")!=NULL) return 7;
    else return 8;
}
/**
 * @brief 套餐触摸入口，选择按钮对应套餐并使用第一个钱包。
 * @param[in] pack 有效的零结尾按钮字符串。
 * @param[in] len 未使用，解析依赖字符串结束符。
 */
void lyy_tjc_touch_package(char *pack,uint16_t len)
{
    (void)len;
    lyy_tjc_packages_choose(get_package_id(pack), 0);
}
/**
 * @brief Get the package id object
 * 
 * @param wallet 
 * @return int 
 */
int get_wallet_id(char *wallet)
{
    if(strstr(wallet,"wallet0")!=NULL) return 0;
    else if(strstr(wallet,"wallet1")!=NULL) return 1;
    else if(strstr(wallet,"wallet2")!=NULL) return 2;
    else if(strstr(wallet,"wallet3")!=NULL) return 3;
    else return 4;
}
/**
 * @brief 
 * 根据钱包显示支付页
 * @param pack 
 * @param len 
 */
void lyy_tjc_touch_wallet(char *pack,uint16_t len)
{

    (void)len;

    int id = get_wallet_id(pack);
    PAY_PARAM_DEF *para = get_pay_param();
    if(id>=para->chnl_sum) //钱包序号不对
    {
        LYY_LOGD("wallet number is error");
        return ;
    }
    lyy_tjc_get_pay_qrcode(pay_url_package_id_get(), id);

}


/**
 * @brief 
 * 
 * @param qr_regist 
 * @param qr_app 
 */
void lyy_tjc_page_obj_regist(char *qr_regist)
{
    lyy_tjc_objTxt("qr_regist", qr_regist);
    vTaskDelay(5 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("title", "Scan the code to bind the device.\\rAfter completing the binding, please restart the device.");
    lyy_tjc_device_infor();
    lyy_tjc_obj_device_infor();            
}

/**
 * @brief 
 * 
 */
void lyy_tjc_page_regist(void)
{
    #define DEV_REGIST_QRCODE   "https://gc.starthing.com/gw/customer/page/scan/"
    char regist_url[128] ={0};
    lyy_tjc_switch_page("regist"); //切换
    sprintf(regist_url,"%s%s",DEV_REGIST_QRCODE,APP_pcGetParamDevId());
    lyy_tjc_page_obj_regist(regist_url);    
}
/**
 * @brief 
 * 
 */
void lyy_tjc_page_regist_set(void)
{
    lyy_tjc_set_status(TJC_SHOW_DEVICE_REGIST); 
}
/**
 * @brief 从屏幕版本字符串更新方向及缓存版本，随后关闭 tm0。
 * @param[in] pack 有效的零结尾版本字符串；长度大于 1 且小于版本缓存容量时保存。
 * @param[in] len 未使用，字符串操作依赖零结尾。
 * @note 仅识别 _00_ 和 _270_ 方向；不在此处发送 OTA 版本上报。
 */
void lyy_tjc_time_version(char *pack,uint16_t len)
{
    (void)len;
    if (strstr(pack, "_00_") != NULL) set_tjc_screen_type(LYY_SCREEN_TYPC_00);
    else if (strstr(pack, "_270_") != NULL) set_tjc_screen_type(LYY_SCREEN_TYPC_270);
    if (strlen(pack) > 1 && strlen(pack) < sizeof(tjc_infor_t.tjc_ver)) {
        memset(tjc_infor_t.tjc_ver, 0, sizeof(tjc_infor_t.tjc_ver));
        strcpy(tjc_infor_t.tjc_ver, pack);
    }
    lyy_tjc_time_en("tm0", 0);
}

/**
 * @brief 先处理图片二进制确认，再按表顺序分发首个匹配的屏幕文本命令。
 * @param[in] packet 原始屏幕数据，NULL 时忽略。
 * @param[in] len 字节数，零长度时忽略。
 * @note 文本最多复制 254 字节并补零，跳过首个零字节；仅做子串匹配，不校验屏幕帧尾。
 */
void tjc_packet_parse(uint8_t *packet, uint16_t len)
{
    char text[255] = {0};
    if (packet == NULL || len == 0) return;
    lyy_display_recvPic(packet, len);
    if (len >= sizeof(text)) len = sizeof(text) - 1;
    if (packet[0] == 0) { packet++; len--; }
    memcpy(text, packet, len);
    for (unsigned int i = 0; i < sizeof(tjc_packet_parse_t) / sizeof(tjc_packet_parse_t[0]); i++) {
        if (strstr(text, tjc_packet_parse_t[i].cmd) != NULL) {
            if (tjc_packet_parse_t[i].parse != NULL) tjc_packet_parse_t[i].parse(text, len);
            break;
        }
    }
}
/**
 * @brief 兼容旧入队名称，实际同步解析屏幕数据。
 * @param[in] pack 接收缓冲区。
 * @param[in] len 有效字节数。
 * @retval -1 指针为空或长度为零。
 * @retval 1 已调用解析，不保证命令被识别或执行成功。
 */
int tjc_packet_queEntry(uint8_t *pack, uint16_t len)
{
    if (pack == NULL || len == 0) return -1;
    tjc_packet_parse(pack, len);
    return 1;
}
/**
 * @brief 保留屏幕出队兼容接口，无实际队列读取。
 * @param[out] unit 未使用，不写入任何数据。
 * @retval -1 固定返回。
 */
int tjc_packet_queOut(STORAGE_QUE_DEF *unit) { (void)unit; return -1; }
/**
 * @brief 解析屏幕型号与旋转方向，更新图片请求的屏幕配置。
 * @param[in] pack 含型号和方向标记的零结尾字符串。
 * @param[in] len 未使用。
 * @note 屏幕版本由独立 screen_ver 命令处理，此处不更新版本。
 */
void lyy_tjc_vlaue_screen_type(char *pack,uint16_t len)
{

    (void)len;
    LYY_LOGD("lyy_tjc_vlaue_screen_type:%s",pack);
    // char ver_screen[50] = {0};
    // char *p = NULL;

    // 获取屏幕型号 TJC8048
    if((strstr(pack,LYY_SCREEN_DEVICE_TJC8048x270) != NULL) || (strstr(pack,LYY_SCREEN_DEVICE_TJC8048x243) != NULL) )
    {
        set_tjc_Screen(LYY_SCREEN_DEVICE);
        // 获取屏幕方向
        if(strstr(pack,"_00_") != NULL) 
        {
            set_tjc_screen_type(LYY_SCREEN_TYPC_H);
        }
        else if(strstr(pack,"_270_") != NULL) 
        {
            set_tjc_screen_type(LYY_SCREEN_TYPC_V);
        }
    }
    else if(strstr(pack,LYY_SCREEN_DEVICE_TJC4880x243) != NULL) 
    {
        set_tjc_Screen(LYY_SCREEN_DEVICE_4880x243);
        // 获取屏幕方向
        if(strstr(pack,"_00_") != NULL) 
        {
            set_tjc_screen_type(LYY_SCREEN_TYPC_00);
        }
        else if(strstr(pack,"_90_") != NULL) 
        {
            set_tjc_screen_type(LYY_SCREEN_TYPC_90);
        }else if(strstr(pack,"_180_") != NULL) 
        {
            set_tjc_screen_type(LYY_SCREEN_TYPC_180);
        }
        else if(strstr(pack,"_270_") != NULL) 
        {
            set_tjc_screen_type(LYY_SCREEN_TYPC_270);
        }
    }

    // 获取屏幕字库
    // tjc_infor_t.screen_type.languages = TJC_ENGLISH;
    /*
    if(strstr(pack,"_QYZ_") != NULL) 
    {
        tjc_infor_t.screen_type.languages = TJC_QYZ;
    }else{
        tjc_infor_t.screen_type.languages = TJC_ENGLISH;
    }
    */
    // p = strchr(pack,':');
    // if(p != NULL)  strcpy(ver_screen,p+1);
    /* Screen version is reported separately by screen_ver. */
}


/**
 * @brief 
 * 判断数据帧格式是否正常
 * @param packet 
 * @param len 
 * @return int 
 * <0 无格式
 * 其他有格式
 */
int tjc_isPacket(uint8_t *packet,uint16_t len)
{

    if(packet[len-1] != 0xff || packet[len-2] != 0xff) return -1;
    if(packet[len-3] != 0xff ) return -1;
    if(packet[0] == 0x70 || packet[0] == 0x71) return 1;
    else return 2;
}


/**
 * @brief 将主板屏幕透传接收数据交给统一解析入口。
 * @param[in] pack 接收数据。
 * @param[in] len 字节数，转交 uint16_t 长度入口时会收窄。
 * @note 当前同步解析，不拷贝入队，不在此处进行拆包或帧尾校验。
 */
void tjc_recv_packet_handle(uint8_t *pack,uint32_t len)
{
    tjc_packet_parse(pack, len);
}


/**
 * @brief 
 * 
 */
void tjc_get_pay_channel_enable(void)
{
    tjc_infor_t.get_pay_channel_flag = 0x01;
}



/**
 * @brief 
 * 显示提示页
 * @param context 
 */
void lyy_tjc_page_waring_init(char *context,uint16_t time)
{
    char temp[10]={0};
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    sprintf(temp,"%dS",time);
    lyy_tjc_objTxt("time", temp);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("body", context);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
}
/**
 * @brief 
 * 获取更新图片结果
 * @param result 
 * @return char* 
 */
char *lyy_tjc_img_result(uint8_t result)
{
    if(result == TJC_PIC_STATE_NONE) return "The results of image updates : idle";
    else if(result == TJC_PIC_STATE_UPDATA) return "The results of image updates : need update";
    else if(result == TJC_PIC_STATE_TRAN) return "The results of image updates : need transfer";
    else if(result == TJC_PIC_RESULT_UPDATA_FAIL) return "The results of image updates : fail";
    else if(result == TJC_PIC_RESULT_HTTP_FAIL) return "The results of image updates : http fail";
    else if(result == TJC_PIC_RESULT_TRAN_FAIL) return "The results of image updates : transfer fail";
    else if(result == TJC_PIC_RESULT_TRAN_SUCCESS) return "The results of image updates : success ";
    else return "The results of image updates : undefine ";
}


/**
 * @brief 
 * 状态页显示
 * @param time 
 * @param title 
 * @param body 
 */
void lyy_tjc_page_obj_status(char *title,char *body,uint16_t time)
{
    char temp[10] = {0};
    // lyy_tjc_switch_page("status");       
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("title", title);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("body", body);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    sprintf(temp,"%dS",time);
    lyy_tjc_objTxt("time", temp);
    vTaskDelay(2 / portTICK_PERIOD_MS);       
}
/**
 * @brief 
 * 
 * @param title 
 * @param body 
 */
void lyy_tjc_page_obj_count_down(char *title,char *body)
{ 
    lyy_tjc_objTxt("title", title);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("body", body);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_device_infor();
    vTaskDelay(2 / portTICK_PERIOD_MS);      
}
/**
 * @brief 
 * 
 * @param body 
 */
void lyy_tjc_obj_update_amy_count_down(char *body)
{
    lyy_tjc_objTxt("body", body);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("net_status", NET_RETURN_RESULT(lyy_mqtt_get_status())); 
    vTaskDelay(2 / portTICK_PERIOD_MS);   
}
/**
 * @brief 
 * 倒计时-按摩椅
 * @param time 
 */
void lyy_tjc_page_amy_time_set(uint32_t time)
{
    if(tjc_infor_t.amy_time.total == 0) //不是累加则需要拿初始值
    {
        tjc_infor_t.amy_time.next_status    = TJC_SHOW_PACKAGES;
        tjc_infor_t.status                  = TJC_SHOW_AMY_TIME_START;    
    }else
    {
        lyy_tjc_switch_page("count_down");
        vTaskDelay(2 / portTICK_PERIOD_MS);
        lyy_tjc_device_infor();
        vTaskDelay(2 / portTICK_PERIOD_MS);
        tjc_infor_t.amy_time.next_status    = TJC_SHOW_PACKAGES;
        tjc_infor_t.status                  = TJC_SHOW_AMY_TIME_WORK;        
    }
    tjc_infor_t.amy_time.total += time;
    LYY_LOGD("tjc_infor_t.amy_time.total:%d",tjc_infor_t.amy_time.total);
}

AMY_WORK_TIME_DEF  amy_time_t;
/**
 * @brief 
 * 按摩椅时间转成字符串显示
 * @param time 
 * @param pstring 
 */
void amy_time_to_string(uint32_t time,char *pstring)
{
    uint32_t hour,min,sec;
    uint32_t temp = 0;
    int32_t value = time;
    hour = value>=3600 ? (value/3600):0; //小时
    // LYY_LOGD("hour : %d",hour);
    temp  = value%3600;  //取余数
    // LYY_LOGD("temp:%d",temp); 
    min = temp >=60 ? temp/60:0; 
    // LYY_LOGD("min : %d",min);
    sec = value%60; //秒 
    sprintf(pstring,"%02ld : %02ld : %02ld",hour,min,sec);   
}


/**
 * @brief 记录毫秒起始时间并进入按摩椅倒计时显示页。
 */
void lyy_tjc_page_change_amy_time(void)
{
    char time[50] = {0};
    amy_time_t.sys_tick_start = HAL_ulGetTimeMs();
    lyy_tjc_switch_page("count_down");
    amy_time_to_string(tjc_infor_t.amy_time.total,time);
    lyy_tjc_page_obj_count_down("Count Down",time); 
    tjc_infor_t.status = TJC_SHOW_AMY_TIME_WORK;   
}
/**
 * @brief 根据 HAL 毫秒时间差换算已用秒数，刷新剩余时间并处理结束状态。
 * @note 每次延时 100 ms，剩余时间未变化时不刷新控件。
 */
void lyy_tjc_page_amy_time_work(void)
{
    char time[50] = {0};
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    amy_time_t.sys_tick_last = HAL_ulGetTimeMs();
    amy_time_t.sys_tick_value = (amy_time_t.sys_tick_last - amy_time_t.sys_tick_start) / 1000;
    amy_time_t.work_value = tjc_infor_t.amy_time.total - amy_time_t.sys_tick_value; //计算剩余时间

    if(amy_time_t.work_value == amy_time_t.last_value) return;//不需要更新
    amy_time_t.last_value = amy_time_t.work_value; //更新
    amy_time_to_string(amy_time_t.work_value,time);

    // LYY_LOGD("sec : %d",sec);
    if(amy_time_t.work_value <= 0) //结束
    {

        vTaskDelay(500 / portTICK_PERIOD_MS); 
        tjc_infor_t.amy_time.total = 0;
        amy_time_t.last_value = 0;
        tjc_infor_t.status = tjc_infor_t.amy_time.next_status;
    }
    lyy_tjc_obj_update_amy_count_down(time);
    // lyy_tjc_page_obj_count_down("Count Down",time); 
}
/**
 * @brief 剩余工作时间不少于 4 秒时恢复倒计时页面。
 * @retval 1 已恢复页面及工作状态。
 * @retval 0 剩余时间不足 4 秒，不更新页面。
 */
int amy_time_redisplay(void)
{
    char time[50] = {0};
    if (amy_time_t.work_value >= 4)
    {
        lyy_tjc_switch_page("count_down");
        amy_time_to_string(amy_time_t.work_value,time);
        lyy_tjc_page_obj_count_down("Count Down",time); 
        tjc_infor_t.status = TJC_SHOW_AMY_TIME_WORK;
        return 0x01;     
    } 
    return 0x00;   
}



/**
 * @brief 
 * 处理串口屏显示状态机
 * @param arg 
 */
void thread_page_display(void *arg)
{

    (void)arg;

    char  txt[5];//加个状态机
    static uint16_t coins_time = 0;
    static uint16_t update_cnt = 0;
    static uint16_t time_cnt = 0;


    while(1)
    {
        switch (tjc_infor_t.status)
        {
            case TJC_SHOW_UPDATE_NET:
            {
                if(update_cnt++ > 200)
                {   
                    update_cnt = 0 ;
                    //更新在线状态
                    lyy_tjc_net_status();
                    //判断倒计时时候还在运行
                    if(amy_time_t.work_value > 0)
                    {
                        lyy_tjc_page_amy_time_work();   
                    }
                }

                vTaskDelay(20 / portTICK_PERIOD_MS);
            }break;
            case TJC_SHOW_PACKAGES:
            {
                lyy_tjc_packages_show();
                memset(&tjc_infor_t.count_down,0x00,sizeof(TJC_COUNT_DOWN_DEF));
                tjc_infor_t.status = TJC_SHOW_UPDATE_NET;
            }break; 
            #if 0
            case TJC_SHOW_PAY: //支付界面
            {
                lyy_tjc_pay_page(&tjc_infor_t.pay);
                lyy_tjc_count_down(tjc_infor_t.pay.time,TJC_SHOW_PACKAGES);
            }break;  
            #endif
    
            case TJC_SHOW_PAY_URL: //显示支付二维码
            {
                lyy_tjc_pay_url_money(&tjc_infor_t.pay);
                if (tjc_infor_t.status == TJC_SHOW_PAY_URL) {
                    lyy_tjc_count_down(tjc_infor_t.pay.time, TJC_SHOW_PACKAGES);
                }
            }break; 
            case TJC_SHOW_PAY_PAGE: //支付界面
            {
                uint8_t wallet_num = tjc_infor_t.pay.wallet_num;
                lyy_tjc_change_pay_page(tjc_infor_t.pay.wallet_sum, wallet_num);
                if (tjc_infor_t.status == TJC_SHOW_PAY_PAGE && tjc_infor_t.pay.wallet_num == wallet_num) {
                    lyy_tjc_count_down(tjc_infor_t.pay.time, TJC_SHOW_PACKAGES);
                }
            }break;  
            case TJC_SHOW_COUNT_DOWN:
            {
                if(tjc_infor_t.count_down.time > 0 && time_cnt++ > 50)
                {
                    time_cnt = 0;
                    memset(txt, 0x00, sizeof(txt));
                    sprintf(txt, "%ldS", tjc_infor_t.count_down.time);
                    lyy_tjc_objTxt("time", txt);
                    // LYY_LOGD("TJC_SHOW_COUNT_DOWN:%d",tjc_infor_t.count_down.time);
                    tjc_infor_t.count_down.time --;
                    //计时结束，回到套餐页面
                    if(tjc_infor_t.count_down.time == 0)
                    {
                        tjc_infor_t.status = tjc_infor_t.count_down.next_status;
                    } 
                    lyy_tjc_net_status();         
                }
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }break;   
            case  TJC_SHOW_WARING: //提示界面
            {
                lyy_tjc_page_obj_waring(tjc_infor_t.waring.context,tjc_infor_t.waring.time);
                lyy_tjc_count_down(tjc_infor_t.waring.time, tjc_infor_t.waring.next_status);
            }break;
            case TJC_SHOW_DBJ_COINS: //出币
            {
                if(tjc_infor_t.dbj_coins.flag == 0x00) 
                {
                    coins_time = tjc_infor_t.dbj_coins.sum+5;
                    tjc_infor_t.dbj_coins.flag = 0x01;
                    lyy_tjc_switch_page("status");
                }      
                lyy_tjc_dbj_coins_out(tjc_infor_t.dbj_coins.sum,tjc_infor_t.dbj_coins.inc,coins_time/5);
                if(tjc_infor_t.dbj_coins.inc == 0)
                {
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                    tjc_infor_t.dbj_coins.flag = 0x00;
                    tjc_infor_t.status = TJC_SHOW_PACKAGES;  //显示套餐       
                }
                else
                {
                    if(coins_time > 0) coins_time --; 
                    else
                    {
                        tjc_infor_t.dbj_coins.flag = 0x00;
                        tjc_infor_t.status = TJC_SHOW_PACKAGES;  //显示套餐    
                    } 
                    vTaskDelay(200 / portTICK_PERIOD_MS); 
                }     
            }break;
            case TJC_SHOW_PAY_RESULT: //支付结果提示
            {
                lyy_tjc_page_obj_pay_result();
                lyy_tjc_count_down(tjc_infor_t.success.time, tjc_infor_t.success.next_status);
            }break;
            case TJC_SHOW_AMY_TIME_START: //开始倒计时
            {
                lyy_tjc_page_change_amy_time();
            }break;
            case TJC_SHOW_AMY_TIME_WORK: //倒计时
            {
                lyy_tjc_page_amy_time_work();       
            }break;

            case TJC_SHOW_LOG_DEBUG: //日志打印
            {
                //todo...
#if 0
                tjc_page_log();
#endif
            }break;
            case TJC_SHOW_DEVICE_REGIST: //注册页面
            {
                lyy_tjc_page_regist();
                tjc_infor_t.status = TJC_SHOW_UPDATE_NET;
            }break;
            case TJC_SHOW_WARING_TIP: // 故障显示
            {
                if(update_cnt++ > 200)
                {   
                    update_cnt = 0 ;
                    //更新在线状态
                    lyy_tjc_net_status();
                }
                vTaskDelay(20 / portTICK_PERIOD_MS); 
            }break;
            default: //未知状态 显示套餐
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                tjc_infor_t.status = TJC_SHOW_PACKAGES;
            }break;
        }
    }
}



/**
 * @brief 执行首页、登录及图片初始化流程，然后进入页面显示循环。
 * @param[in] arg 未使用，可为 NULL。
 * @note 图片处理直接同步调用，结束后显示其结果，不另起图片任务。
 */
void thread_screen_init(void *arg)
{

    (void)arg;
    int ret = 0;

    
    while(1)
    {
        switch(tjc_infor_t.step)
        {
            case SCREEN_START_STEP_HOMEPAGE: //显示home page
            {
                LYY_LOGD("home page");
                vTaskDelay(500 / portTICK_PERIOD_MS); 
                lyy_tjc_switch_page("homepage");        //切换到首页
                vTaskDelay(500 / portTICK_PERIOD_MS); 
                lyy_tjc_switch_page("homepage");        //切换到首页
                lyy_tjc_screen_type_get();              //屏型号
                lyy_tjc_page_homepage();                //
                vTaskDelay(3000 / portTICK_PERIOD_MS);  
                tjc_infor_t.step = SCREEN_START_STEP_LOGINING; //
            }break;
            case SCREEN_START_STEP_LOGINING: //显示登录页面
            {
                lyy_tjc_page_logining(); //
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                if(tjc_infor_t.get_pay_channel_flag == 0x01)
                {
                    tjc_infor_t.step = SCREEN_START_STEP_SEND_PIC; //更新图片
                }
                else if(tjc_infor_t.get_regist_flag == 0x01)
                {
                    tjc_infor_t.step = SCREEN_START_STEP_REGIST; //更新图片
                }  
            }break;
            case SCREEN_START_STEP_SEND_PIC: //
            {

                LYY_LOGD("img update");
                lyy_tjc_switch_page("waring");        //切换到首页
                lyy_tjc_page_waring_init("Image updates : start",300); //5分钟
                // time = 300;
                vTaskDelay(500 / portTICK_PERIOD_MS); 
                ret = lyy_start_pic_get(); //更新图片
                if(ret < 0 ) 
                {
                    lyy_tjc_page_waring_init("Image updates : parameter is errer",2);
                    vTaskDelay(1000 / portTICK_PERIOD_MS); 
                }
                else
                {

                    thread_tjcPicDealThread(NULL);
                    lyy_tjc_page_waring_init(lyy_tjc_img_result(tjc_infor_t.img_result), 4);
                }
                tjc_infor_t.step = SCREEN_START_STEP_END; //更新图片

            }break;
            case SCREEN_START_STEP_REGIST: //注册页面
            {
                lyy_tjc_page_regist();
                vTaskDelay(2000 / portTICK_PERIOD_MS); 
                tjc_infor_t.step = SCREEN_START_STEP_UPDATE_NET; //更新图片
            }break;
            case SCREEN_START_STEP_UPDATE_NET:
            {
                lyy_tjc_device_infor(); 
                vTaskDelay(4000 / portTICK_PERIOD_MS);   
            }break;
            default:
            {
            }break;
        }   

        if(tjc_infor_t.step == SCREEN_START_STEP_END)
        {
            break;
        }
 
    }

    thread_page_display(NULL);

}


/**
 * @brief 初始化图片信号量、默认屏幕版本及首页步骤，创建屏幕任务。
 * @note 默认版本为 tjc_default_v3.0.0；任务内部同步执行图片更新后进入页面显示循环。
 * @warning 不检查信号量或任务创建结果，调用方不得将返回视为初始化成功确认。
 */
void lyy_init_tjc(void)
{

    lyy_picInit();
    tjc_infor_t.step = SCREEN_START_STEP_HOMEPAGE;
    strcpy(tjc_infor_t.tjc_ver, "tjc_default_v3.0.0");
    
#define SCAN_UART_BRT               115200
#define TJC_THREAD_NAME             "TJC_THREAD"
#define TJC_THREAD_STACK (1024 * 6 / sizeof(portSTACK_TYPE))



    xTaskCreate(    thread_screen_init, 
                    TJC_THREAD_NAME, 
                    TJC_THREAD_STACK, 
                    NULL, 
                    5,
                    NULL);

}
