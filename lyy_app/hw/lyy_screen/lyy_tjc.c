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
#include "hal_sem.h"
#include "queue.h"
#include "HAL_Flash.h"

#include "thing_mqtt.h"
#include "thing_event.h"
#include "thing_property.h"
#include "thing_iot.h"
#include "thing_init.h"
#include "lyy_device.h"

#include "lyy_tjc.h"
#include "lyy_pic.h"
#include "lyy_para.h"
#include "lyy_pulses.h"
#include "lyy_start.h"
#include "lyy_amy.h"

#include "USER_Config.h"



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
    SCREEN_START_STEP_OTA,
} SCREEN_START_STEP;

TJC_INFOR_DEF   tjc_infor_t;

static uint8_t      g_SemSta;
SemaphoreHandle_t   hw_httpDowloadSem    = NULL;
int baudTable[3] = {115200, 9600, 921600};
static SEM_TYPE screen_ota_connect_cmd_send = NULL;
static SEM_TYPE screen_ota_baud_set = NULL;
static SEM_TYPE screen_ota_ota_cmd_send = NULL;

enum
{
    PULSES_TOUCH_POSITION,
    PULSES_TOUCH_PACKAGE,
    PULSES_TOUCH_WALLET,
};

typedef struct
{
    uint32_t generation;
    uint8_t kind;
    uint8_t index;
} TJC_PULSES_TOUCH;

typedef struct
{
    char id[sizeof(((PAY_PACKAGE_DEF *)0)->id)];
    char currency[64];
} TJC_PULSES_SLOT;

typedef struct
{
    char url[sizeof(tjc_infor_t.pay.url)];
    char money[sizeof(tjc_infor_t.pay.money)];
    char currency[sizeof(tjc_infor_t.pay.currency_desc)];
} TJC_PULSES_QR;

static QueueHandle_t pulses_touches;
static QueueHandle_t pulses_qr_replies;
/* Shared between the display task and the MQTT QR callback. */
static char pulses_qr_token[13];
/* UART RX stamps touches; only the display task changes the generation and owns UI state. */
static uint32_t pulses_generation;
static uint32_t pulses_revision;
static uint8_t pulses_on_packages;
static uint8_t pulses_position;
static TJC_PULSES_SLOT pulses_slots[PULSES_PACKAGE_MAX];
static struct
{
    uint8_t position;  /* 0 when no active payment */
    uint8_t pending;
    TJC_PULSES_SLOT slot;
    TJC_PULSES_QR reply;
} pulses_payment;


void lyy_tjc_pay_package_ndj(void);
void lyy_tjc_page_pkgs_ndj(void);
void lyy_tjc_page_pkgs_std(void);
void lyy_tjc_page_pkgs_pulse(void);
void lyy_tjc_page_pkgs_xhj(void);
void lyy_tjc_page_change_amy_time(void);

void lyy_tjc_touch_pay_deduction(char *pack, uint16_t len);
void lyy_tjc_touch_pay_now(char *pack, uint16_t len);
void lyy_tjc_touch_earn_points(char *pack, uint16_t len);
void tjc_touch_return(char *pack, uint16_t len);
void lyy_tjc_touch_time_return(char *pack,uint16_t len);
void tjc_touch_back(char *pack,uint16_t len);
void lyy_tjc_touch_package(char *pack,uint16_t len);
void lyy_tjc_touch_wallet(char *pack,uint16_t len);
void lyy_tjc_time_version(char *pack,uint16_t len);
void lyy_tjc_vlaue_screen_type(char *pack,uint16_t len);
void tjc_touch_regist_next(char *pack,uint16_t len);
void tjc_touch_device_status_display(char *pack,uint16_t len);
void lyy_tjc_touch_pay_now_points(char *pack, uint16_t len);

const TJC_PACKET_PARSE_DEF tjc_packet_parse_t[] =
    {
        {"pay_deduction", lyy_tjc_touch_pay_deduction},
        {"pay_now_points", lyy_tjc_touch_pay_now_points},
        {"pay_now", lyy_tjc_touch_pay_now},
        {"earn_points", lyy_tjc_touch_earn_points},
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
 * @brief 
 * 获取积分配置
 * @return 
 */
TJC_POINT_CONFIG_DEF *get_point_config(void)
{
    return &tjc_infor_t.point_config;
}

/**
 * @brief
 * 获取积分授权
 * @return
 */
TJC_POINT_AUTHORIZE_DEF *get_point_authorize(void)
{
    return &tjc_infor_t.point_authorize;
}

/**
 * @brief
 * 获取积分领取
 * @return
 */
TJC_POINT_CLAIM_DEF *get_point_claim(void)
{
    return &tjc_infor_t.point_claim;
}

/**
 * @brief
 * 设备可以领取积分展示
 * @return
 */
void device_get_points_show(void)
{
    if (get_dev_type_code() == DEVICE_TYPE_PULSES) return;
    char str[200] = {0};

    TJC_POINT_CONFIG_DEF *point_config_data = get_point_config();

    // 拼接样例  100.02Pt=1RMB
    sprintf(str, "%sPt=%s%s", point_config_data->equipment_available_point, point_config_data->equipment_available_deduct_amount, point_config_data->currency_code);

    lyy_tjc_objTxt("get_points", str);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

/**
 * @brief
 * 会员现有积分展示
 * @return
 */  
void member_now_points_show(void)
{

    char str[200] = {0};
    char str_2[200] = {0};

    TJC_POINT_CONFIG_DEF *point_config_data = get_point_config();
    TJC_POINT_CLAIM_DEF *point_claim_data = get_point_claim();

    // 拼接样例  100.02Pt=1RMB
    sprintf(str, "%sPt=%s%s", point_claim_data->user_point, point_claim_data->user_deduct_amount, point_config_data->currency_code);
    lyy_tjc_objTxt("now_points", str);    
    vTaskDelay(2 / portTICK_PERIOD_MS);

    // 拼接样例  +0.02Pt Received
    sprintf(str_2, "+%sPt Received", point_claim_data->claimed_points);
    lyy_tjc_objTxt("add_points", str_2);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

/**
 * @brief
 * 积分最大可以抵扣多少钱展示
 * @return
 */
void points_deduction_max_amount_show(void)
{
    if (get_dev_type_code() == DEVICE_TYPE_PULSES) return;

    char str[200] = {0};
    char str_2[200] = {0};
    TJC_POINT_CONFIG_DEF *point_config_data = get_point_config();

    if (get_dev_type_code() == DEVICE_TYPE_UART_NDJ)
    {
        PAY_PACKAGES_NDJ_DEF *pkgs = get_pay_ndj_packages();

        // 拼接样例  RMB
        sprintf(str, "%s", point_config_data->currency_code);
        lyy_tjc_objTxt("currency_code", str);
        vTaskDelay(2 / portTICK_PERIOD_MS);

        // 拼接样例  0.02
        sprintf(str_2, "%s", pkgs->lane[tjc_infor_t.package_record].pkg.orderMaxDeductAmount);
        lyy_tjc_objTxt("MaxDeductAmout", str_2);
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
    else
    {
        PAY_PACKAGES_DEF *pkgs = get_pay_packages();

        // 拼接样例  RMB
        sprintf(str, "%s", point_config_data->currency_code);
        lyy_tjc_objTxt("currency_code", str);
        vTaskDelay(2 / portTICK_PERIOD_MS);

        // 拼接样例  0.02
        sprintf(str_2, "%s", pkgs->pkg[tjc_infor_t.package_record].orderMaxDeductAmount);
        lyy_tjc_objTxt("MaxDeductAmout", str_2);
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief
 * 会员去消费积分展示  
 * @return
 */
void member_to_consume_points_show(void)
{
    char str[200] = {0};
    char str_2[200] = {0};
    char str_3[200] = {0};
    char str_4[200] = {0};

    TJC_POINT_CONFIG_DEF *point_config_data = get_point_config();
    TJC_POINT_AUTHORIZE_DEF *point_authorize_data = get_point_authorize();

    // 拼接样例  100.02Pt=1RMB
    sprintf(str, "%sPt=%s%s", point_authorize_data->user_point, point_authorize_data->user_deduct_amount, point_config_data->currency_code);
    lyy_tjc_objTxt("consume_points", str);
    vTaskDelay(2 / portTICK_PERIOD_MS);

    // 拼接样例  Pay Deduction 100HKD  抵扣金额
    sprintf(str_2, "Pay Deduction\\r%s%s", point_authorize_data->pay_amount, point_config_data->currency_code);
    lyy_tjc_objTxt("deduc_amount", str_2);
    vTaskDelay(2 / portTICK_PERIOD_MS);

    // 拼接样例  Pay Now 100HKD   正常金额
    sprintf(str_3, "Pay Now\\r%s%s", point_authorize_data->order_amount, point_config_data->currency_code);
    lyy_tjc_objTxt("now_amount", str_3);
    vTaskDelay(2 / portTICK_PERIOD_MS);

    // 拼接样例  +0.02Pt Received   登录消费领取积分
    sprintf(str_4, "+%sPt Received", point_authorize_data->claimed_points);
    lyy_tjc_objTxt("reg_add_points", str_4);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

/**
 * @brief
 * 会员去消费 增加 积分展示  
 * @return
 */
void member_to_consume_add_points_show(void)
{

    char str[200] = {0};
    //TJC_POINT_AUTHORIZE_DEF *point_authorize_data = get_point_authorize();

    // 拼接样例  +0.02Pt Received  支付成功，直接到账积分
    sprintf(str, "+%sPt Received", tjc_infor_t.pay_claimed_points);
    lyy_tjc_objTxt("pay_add_points", str);
    vTaskDelay(2 / portTICK_PERIOD_MS);                                   
}

/**
 * @brief
 * 游客支付，显示待领取积分  和可抵扣金额
 * @return
 */
void tourist_to_consume_add_points_show(void)
{
    if (get_dev_type_code() == DEVICE_TYPE_PULSES) return;

    char str[200] = {0};
    char str_2[200] = {0};

    TJC_POINT_CONFIG_DEF *point_config_data = get_point_config();

    // 拼接样例Pending points  100.02Points
    // sprintf(str, "Pending points\\r%sPoints", tjc_infor_t.pay_claimed_points);
    // lyy_tjc_objTxt("pending_points", str);
    // vTaskDelay(5 / portTICK_PERIOD_MS);

    // 拼接样例 100.02Pt
    sprintf(str, "%sPt", point_config_data->equipment_available_point);
    lyy_tjc_objTxt("pending_points", str);
    vTaskDelay(5 / portTICK_PERIOD_MS);

    // 拼接样例 Convertible to 10RMB
    sprintf(str_2, "Convertible to %s%s", point_config_data->equipment_available_deduct_amount, point_config_data->currency_code);
    lyy_tjc_objTxt("convertible", str_2);
    vTaskDelay(5 / portTICK_PERIOD_MS);
}

/**
 * @brief
 * 游客支付，显示待领取积分  和可抵扣金额  loading(支付成功时，不会立刻显示所以添加loading字符)
 * @return
 */
void tourist_to_consume_add_points_show_loading(void)
{

    char str[200] = {0};

    sprintf(str, "%s", "Loading");
    lyy_tjc_objTxt("pending_points", str);
    vTaskDelay(10 / portTICK_PERIOD_MS);
}

/**
 * @brief
 * 本次消费可以增加积分tjc_infor_t.pay_claimed_points
 * @return
 */
void consume_can_add_points_show(void)
{

    char str[200] = {0};
    //TJC_POINT_AUTHORIZE_DEF *point_authorize_data = get_point_authorize();

    // 拼接样例  
    sprintf(str, "You will earn points after successful payment");
    lyy_tjc_objTxt("pay_add_points", str);
    vTaskDelay(2 / portTICK_PERIOD_MS);
}

/**
 * @brief
 * 发送数据到串口屏
 * @param objName 对象名称
 * @param txt 对象内容
 */
void lyy_tjc_cmd_send(char *data, uint16_t len)
{
    HAL_vUartSend(HAL_UART_SCREEN, (uint8_t *)data, len, 500);
}

/**
 * @brief 
 * taojingchi串口屏控件txt修改
 * @param objName 对象名称
 * @param txt 对象内容
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
}

/* Like the existing TJC text path, pass through the cloud's UTF-8 bytes.
 * Replace unsafe ASCII/terminator bytes before putting text in a TJC string literal. */
static void tjc_pulses_safe_text(char *text)
{
    for (unsigned char *p = (unsigned char *)text; *p; ++p)
    {
        if (*p < 0x20 || *p == 0x7f || *p == 0xff || *p == '"' || *p == '\\')
            *p = ' ';
    }
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
    lyy_tjc_objTxt("telephone", tjc_infor_t.telephone);
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
 * @brief 
 * 显示金额和支付二维码
 * @param pay 
 */
void lyy_tjc_pay_url_money(PTJC_PAY_PAGE_DEF pay)
{
    if (get_dev_type_code() == DEVICE_TYPE_PULSES)
    {
        char amount[sizeof(pulses_payment.reply.money) + sizeof(pulses_payment.slot.currency) + 1];
        const char *currency = pulses_payment.reply.currency[0] ?
            pulses_payment.reply.currency : pulses_payment.slot.currency;
        snprintf(amount, sizeof(amount), "%s%s%s", pulses_payment.reply.money,
                 currency[0] ? " " : "", currency);
        tjc_pulses_safe_text(amount);
        lyy_tjc_objTxt("money", amount);
        vTaskDelay(5 / portTICK_PERIOD_MS);
        lyy_tjc_objTxt("pay_prompt", "Scan the QR code to pay");
        vTaskDelay(5 / portTICK_PERIOD_MS);
        lyy_tjc_objTxt("pay_url", pulses_payment.reply.url);
        lyy_tjc_objHide("pay_url", 1);
        return;
    }
    char money_unit[128] = {0};
    char currency[10] = {0};
    if (tjc_infor_t.pay.currency_desc[0] == '\0')   //为了解决货币单位任意变化的问题，由后台下发
    {
        strcpy(currency, get_money_unit());
    }
    else
    {
        strcpy(currency, tjc_infor_t.pay.currency_desc);
    }
    // char page_name[20] = {0};
    sprintf(money_unit, "%s %s", pay->money, currency);
    lyy_display_showpic("pay",tjc_infor_t.pay.wallet_num+1);
    vTaskDelay(5 / portTICK_PERIOD_MS);  
    lyy_tjc_objTxt("money", money_unit);
    vTaskDelay(5 / portTICK_PERIOD_MS);
    if (tjc_infor_t.point_config.point_enabled == 1) // 积分固件兼容
    {
        lyy_tjc_objTxt("pay_prompt", "Earn points for successful payment");
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
    else
    {
        lyy_tjc_objTxt("pay_prompt", "Scan the QR code to pay");
        vTaskDelay(5 / portTICK_PERIOD_MS);
    } 
    LYY_LOGD("lyy_tjc_pay_url_money money:%s",money_unit);    
    if(strlen(pay->url) != 0)
    {
        LYY_LOGD("lyy_tjc_pay_url_money pay_url:%s",pay->url);
        lyy_tjc_objTxt("pay_url", pay->url); 
        vTaskDelay(5 / portTICK_PERIOD_MS);  
    }
    
    lyy_tjc_device_infor();
}
/**
 * @brief 
 * 切换到现实支付二维码界面
 * @param wallet_sum 
 * @param wallet_num 
 */
void lyy_tjc_change_pay_page(uint8_t wallet_sum,uint8_t wallet_num)
{
    char page_name[20] = {0}; 
    sprintf(page_name,"pay%d",wallet_sum);
    lyy_tjc_switch_page(page_name);
    if (get_dev_type_code() == DEVICE_TYPE_PULSES)
        lyy_tjc_objHide("pay_url", 0);
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
/* Only the current dual-lane QR request may publish a reply. */
int lyy_tjc_pulses_qr_reply(const char *token, const char *url, const char *money, const char *currency)
{
    TJC_PULSES_QR reply = {0};
    if (!pulses_qr_replies || !token || !*token || !url || !*url || !money ||
        strlen(url) >= sizeof(reply.url) || strlen(money) >= sizeof(reply.money)) return 0;
    for (const unsigned char *p = (const unsigned char *)url; *p; ++p) {
        if (*p <= 0x20 || *p == 0x7f || *p == 0xff || *p == '"' || *p == '\\') return 0;
    }
    strcpy(reply.url, url);
    strcpy(reply.money, money);
    if (currency) snprintf(reply.currency, sizeof(reply.currency), "%s", currency);

    /* Matched and queued under the same lock as token replacement. A late
     * reply cannot overwrite a new wallet's QR, even during the switch. */
    taskENTER_CRITICAL();
    int ok = strcmp(token, pulses_qr_token) == 0 && xQueueOverwrite(pulses_qr_replies, &reply) == pdPASS;
    taskEXIT_CRITICAL();
    return ok;
}

void lyy_tjc_pay_url_show(char *url,char *money)
{
    if (get_dev_type_code() == DEVICE_TYPE_PULSES) return;
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
static void tjc_pulses_render(void)
{
    char names[PULSES_LANE_COUNT][128] = {{0}};
    uint8_t lane_ready = 0;
    /* Odd generation while painting: no package tap can select a partially rendered slot. */
    __atomic_add_fetch(&pulses_generation, 1, __ATOMIC_RELAXED);
    pulses_revision = pulses_get_revision();
    for (uint8_t position = 1; position <= PULSES_LANE_COUNT; ++position)
    {
        char *name = names[position - 1];
        if (pulses_get_lane_name(position, name, sizeof(names[0])) == 1)
        {
            name[sizeof(names[0]) - 1] = '\0';
            tjc_pulses_safe_text(name);
            if (position == pulses_position) lane_ready = 1;
        }
        else
            name[0] = '\0';
    }
    lyy_tjc_objTxt("t1", names[0]);
    lyy_tjc_objTxt("t3", names[1]);
    lyy_tjc_objHide("t0", pulses_position == 1);
    lyy_tjc_objHide("t2", pulses_position == 2);

    uint8_t count = lane_ready ? pulses_get_package_count(pulses_position) : 0;
    memset(pulses_slots, 0, sizeof(pulses_slots));
    for (uint8_t i = 0; i < PULSES_PACKAGE_MAX; ++i)
    {
        PAY_PACKAGE_DEF pkg = {0};
        TJC_PULSES_SLOT *slot = &pulses_slots[i];
        char obj[16];
        char cmd[32];
        uint8_t visible = i < count && pulses_touches != NULL && pulses_qr_replies != NULL &&
            pulses_get_package(pulses_position, i, &pkg, slot->currency, sizeof(slot->currency)) == 1 &&
            pkg.id[0] != '\0' && memchr(pkg.id, '\0', sizeof(pkg.id)) != NULL;
        snprintf(obj, sizeof(obj), "pkg%d", i + 1);
        if (visible)
        {
            char first[100], second[100];
            memcpy(slot->id, pkg.id, sizeof(slot->id));
            slot->currency[sizeof(slot->currency) - 1] = '\0';
            if (pkg.commodity_Desc[0] || pkg.price_Desc[0])
            {
                snprintf(first, sizeof(first), "%.*s", (int)sizeof(pkg.commodity_Desc), pkg.commodity_Desc);
                snprintf(second, sizeof(second), "%.*s", (int)sizeof(pkg.price_Desc), pkg.price_Desc);
            }
            else
            {
                snprintf(first, sizeof(first), "%.*s %s", (int)sizeof(pkg.display_price),
                         pkg.display_price, slot->currency);
                snprintf(second, sizeof(second), "%.*s %.*s", (int)sizeof(pkg.display_value),
                         pkg.display_value, (int)sizeof(pkg.unit), pkg.unit);
            }
            tjc_pulses_safe_text(first);
            tjc_pulses_safe_text(second);
            char text[sizeof(first) + sizeof(second) + 1];
            snprintf(text, sizeof(text), "%s\\r%s", first, second);
            lyy_tjc_objTxt(obj, text);
        }
        lyy_tjc_objHide(obj, visible);
        int len = snprintf(cmd, sizeof(cmd), "tsw %s,%d\xff\xff\xff", obj, visible);
        lyy_tjc_cmd_send(cmd, len);
    }
    __atomic_add_fetch(&pulses_generation, 1, __ATOMIC_RELAXED);
}

static void tjc_pulses_enter_packages(void)
{
    pulses_position = 1;
    pulses_on_packages = 0;
    if (pulses_touches != NULL) xQueueReset(pulses_touches);
    lyy_tjc_switch_page("pkg_std2");
    lyy_tjc_device_infor();
    tjc_pulses_render();
    pulses_on_packages = 1;
}

/**
 * @brief 
 * 
 */
void lyy_tjc_packages_show(void)
{
    if (get_dev_type_code() == DEVICE_TYPE_PULSES)
    {
        tjc_pulses_enter_packages();
        return;
    }
    #ifdef _PIC_PKG_
    switch (get_dev_type_code())
    {

        case DEVICE_TYPE_UART_NDJ: lyy_tjc_pay_package_ndj();break;
        case DEVICE_TYPE_UART_XYJ: lyy_tjc_package_page();break;
        default:lyy_tjc_package_page();break;
    }
    #else
    switch (get_dev_type_code())
    {
        case DEVICE_TYPE_UART_SKYWORTH_GTXYJ:
        case DEVICE_TYPE_UART_SKYWORTH_GTHGJ:
        case DEVICE_TYPE_UART_SKYWORTH_GTXHJ:
        case DEVICE_TYPE_UART_MSHGJ:
        case DEVICE_TYPE_UART_TCLXYJ:
        case DEVICE_TYPE_UART_DFSSJ:   //该处待议，现在还没有定义TJC屏幕上显示什么
            lyy_tjc_page_pkgs_xhj();
            break;
        case DEVICE_TYPE_UART_NDJ:
            lyy_tjc_page_pkgs_ndj();
            break;
        case DEVICE_TYPE_UART_XYJ:
        case DEVICE_TYPE_UART_THXCJ:
            lyy_tjc_page_pkgs_std();
            break;
        case DEVICE_TYPE_PULSE:
            lyy_tjc_page_pkgs_pulse();
            break;
        default:
            lyy_tjc_page_pkgs_std();
            break;
    }
    #endif
}
/**
 * @brief
 * 更改显示套餐颜色   color_str
 * @param
 * @param
 */
void lyy_tjc_pkg_set_color(char *color_str, uint8_t pkg_num)
{
    char str[10]={0};
    char cmdBuff[128] = {0};

    if (color_str == NULL || strlen(color_str) != 7 || color_str[0] != '#')
    {
        LYY_LOGD("NO #RRGGBB");
        return;
    }

    strcpy(str, color_str);

    // 提取 R、G、B 三个通道的16进制值
    uint8_t r = (str[1] >= 'A' ? (str[1] - 'A' + 10) : (str[1] - '0')) * 16 + (str[2] >= 'A' ? (str[2] - 'A' + 10) : (str[2] - '0'));
    uint8_t g = (str[3] >= 'A' ? (str[3] - 'A' + 10) : (str[3] - '0')) * 16 + (str[4] >= 'A' ? (str[4] - 'A' + 10) : (str[4] - '0'));
    uint8_t b = (str[5] >= 'A' ? (str[5] - 'A' + 10) : (str[5] - '0')) * 16 + (str[6] >= 'A' ? (str[6] - 'A' + 10) : (str[6] - '0'));

    // 转换成 RGB565
    uint16_t rgb565 = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);

    sprintf(cmdBuff, "pkg%d.bco=%d\xff\xff\xff", pkg_num, rgb565);

    LYY_LOGD("rgb565[%s]", cmdBuff);

    lyy_tjc_cmd_send(cmdBuff, strlen(cmdBuff));
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
 * @brief
 * 洗衣机烘干机显示套餐页
 * @param money
 * @param value
 * @param pkg_num
 */
void lyy_tjc_page_pkgs_xhj(void)
{
    PAY_PACKAGES_DEF *pkgs = get_pay_packages();
    // char price[128]={0};
    char money[128] = {0};
    char mode_name[128] = {0};
    uint8_t i = 0;
    if (tjc_infor_t.point_config.point_enabled == 1) // 积分固件兼容
    {
        lyy_tjc_switch_page("pkg_std2_p"); // 切换页面
    }
    else
    {
        lyy_tjc_switch_page("pkg_std2"); // 切换页面
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lyy_tjc_device_infor();
    if (pkgs->sum < 6) // 隐藏不需要的套餐
    {
        for (i = pkgs->sum + 1; i <= 6; i++) // 套餐ID 从1 存储也是从1开始
        {
            lyy_tjc_clean_pkg_std(i); // 隐藏
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    for (i = 0; i < pkgs->sum; i++) //
    {
        lyy_tjc_vis_pkg_std(i + 1);
        sprintf(money, "%s %s", pkgs->pkg[i].display_price, pkgs->currency_name);
        // sprintf(price,"%s %s",pkgs->pkg[i].display_value,pkgs->pkg[i].unit);
        sprintf(mode_name, "%s", pkgs->pkg[i].name);
        lyy_tjc_pkg_std(money, mode_name, i + 1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
/**
 * @brief 
 * 显示套餐页
 * @param money 
 * @param value 
 * @param pkg_num 
 */
void lyy_tjc_page_pkgs_std(void)
{
    PAY_PACKAGES_DEF *pkgs = get_pay_packages();
    char price[128]={0};
    char money[128]={0};
    uint8_t i = 0;

    if (tjc_infor_t.point_config.point_enabled == 1)  //积分固件兼容
    {
        lyy_tjc_switch_page("pkg_std2_p"); // 切换页面
    }
    else
    {
        lyy_tjc_switch_page("pkg_std2"); // 切换页面
    }

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
 * @brief
 * 显示套餐页----脉冲
 * @param money
 * @param value
 * @param pkg_num
 */
void lyy_tjc_page_pkgs_pulse(void)
{
    PAY_PACKAGES_DEF *pkgs = get_pay_packages();
    uint8_t i = 0;
    if (tjc_infor_t.point_config.point_enabled == 1) // 积分固件兼容
    {
        lyy_tjc_switch_page("pkg_std2_p"); // 切换页面
    }
    else
    {
        lyy_tjc_switch_page("pkg_std2"); // 切换页面
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
    lyy_tjc_device_infor();
    if (pkgs->sum < 6) // 隐藏不需要的套餐
    {
        for (i = pkgs->sum + 1; i <= 6; i++) // 套餐ID 从1 存储也是从1开始
        {
            lyy_tjc_clean_pkg_std(i); // 隐藏
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
    for (i = 0; i < pkgs->sum; i++) //
    {
        lyy_tjc_vis_pkg_std(i + 1);
        vTaskDelay(10 / portTICK_PERIOD_MS);

        // 给套餐发颜色
        lyy_tjc_pkg_set_color(pkgs->pkg[i].package_Color, i + 1);

        //sprintf(money, "%s %s", pkgs->pkg[i].display_price, pkgs->currency_name);
        //sprintf(price, "%s %s", pkgs->pkg[i].display_value, pkgs->pkg[i].unit);
        lyy_tjc_pkg_std(pkgs->pkg[i].commodity_Desc, pkgs->pkg[i].price_Desc, i + 1);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

/**
 * @brief 
 * 显示套餐---扭蛋机
 * @param name 
 * @param money 
 * @param value 
 * @param pkg_num 
 */
void lyy_tjc_pkg_ndj(char *name,char *money,char *value,uint8_t pkg_num)
{

    (void)value;

    char pkg[20]={0};
    char txt[128] = {0};
    char num[20] = {0};
    sprintf(pkg,"pkg%d",pkg_num);
    sprintf(num,"NO.%d",pkg_num);
    sprintf(txt,"  %s\\r  %s\\r  %s",num,money,name);
    lyy_tjc_objTxt(pkg, txt); 

}

/**
 * @brief 
 * 显示套餐页---扭蛋机
 * @param money 
 * @param value 
 * @param pkg_num 
 */
void lyy_tjc_page_pkgs_ndj(void)
{
    PAY_PACKAGES_NDJ_DEF *pkgs = get_pay_ndj_packages();
    PLANE_INFOR_TABLE_DEF param = ndj_net_lane_infor();
    char price[128]={0};
    char money[128]={0};
    uint8_t i = 0;
    if (tjc_infor_t.point_config.point_enabled == 1) // 积分固件兼容
    {
        lyy_tjc_switch_page("pkg_ndj2_p"); // 切换页面
    }
    else
    {
        lyy_tjc_switch_page("pkg_ndj2"); // 切换页面
    }
    vTaskDelay(10 / portTICK_PERIOD_MS); 
    lyy_tjc_device_infor();
    LYY_LOGD("lyy_tjc_page_pkgs_ndj");
    if(pkgs->lane_sum < NDJ_LANE_MAX)//隐藏不需要的套餐
    {
        for(i=pkgs->lane_sum + 1;i<NDJ_LANE_MAX;i++)
        {
            lyy_tjc_clean_pkg_std(i); //隐藏
            vTaskDelay(10 / portTICK_PERIOD_MS);  
        }
        LYY_LOGD("lyy_tjc_page_pkgs_ndj clean[sum]= %d",pkgs->lane_sum);
    }
    
    for(i=0;i<pkgs->lane_sum;i++) //
    {
        if(ndj_get_lane_enable(i) == 0x01) 
        {
            LYY_LOGD("lane[%d]  is enable",i);
            lyy_tjc_vis_pkg_std(i+1);       // 隐藏控件名称空1开始
            vTaskDelay(10 / portTICK_PERIOD_MS);  
            sprintf(money,"%s %s",pkgs->lane[i].pkg.display_price,pkgs->currency_name);
            sprintf(price,"%s %s",pkgs->lane[i].pkg.display_value,pkgs->lane[i].pkg.unit);
            lyy_tjc_pkg_ndj(param->lane[i].giftName,money,price,i+1); // 控件名字加1
            vTaskDelay(10 / portTICK_PERIOD_MS);  
        }
        else 
        {
            lyy_tjc_clean_pkg_std(i+1);       //隐藏
            vTaskDelay(10 / portTICK_PERIOD_MS);  
        }
    } 
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
 * @brief 
 * 
 * @param num_page 
 */
void lyy_tjc_pay_package_ndj(void)
{
    PAY_PACKAGES_NDJ_DEF *infor = get_pay_ndj_packages();
    PLANE_INFOR_TABLE_DEF lane_infor = ndj_net_lane_infor();
    uint8_t enable = 0;
    int8_t num = lane_infor->number;  //按照货道数显示  需要显示得数量
    lyy_tjc_switch_page("package_ndj");
    lyy_tjc_device_infor();
    for(uint8_t i=1;i<=4;i++) //先清除旧套餐
        lyy_tjc_ndj_clean_price(i);
    for(uint8_t i= 1;i<=num;i++) //显示套餐金额
    {
        enable = ndj_get_lane_enable(i); //禁止则展示 “disable”
        // LYY_LOGD("lane[%d] enable :%d",i,enable);
        // LYY_LOGD("infor->lane[%d].package[0].price :%8.2f",i,infor->lane[i].package[0].price);
        // LYY_LOGD("infor->lane[%d].giftName :%s",i,lane_infor->lane[i].giftName);
        // LYY_LOGD("infor->lane[%d].x :%d",i,infor->lane[i].x);
        if(enable == 0x01)
            lyy_tjc_ndj_priceShow(infor->lane[i].x,infor->currency_name,infor->lane[i].pkg.price,lane_infor->lane[i].giftName);
    }
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
 * @brief 
 * 显示支付结果
 */
void lyy_tjc_page_pay_reuslt(void)
{

    if (tjc_infor_t.point_config.point_enabled == 1)
    {
        lyy_tjc_pay_result(40, TJC_SHOW_PACKAGES);
    }
    else
    {
        lyy_tjc_pay_result(10, TJC_SHOW_PACKAGES);
    }
      
}

void lyy_tjc_page_earn_points_url_show(void)
{
#ifdef URL_TYPE_DEVELOP
    // 开发环境
#define DEV_URL_EARN_POINTS_START "https://dc.cadmem.com/gw/customer/page/scan/" // 开发地址

#else

    // 正式环境
#define DEV_URL_EARN_POINTS_START "https://gc.starthing.com/gw/customer/page/scan/" // 生产地址

#endif

#define DEV_URL_EARN_POINTS_END "?scene=marketPointClaim"

    char earn_points_url[200] = {0};
    sprintf(earn_points_url, "%s%s%s", DEV_URL_EARN_POINTS_START, APP_pcGetParamDevId(), DEV_URL_EARN_POINTS_END);

    lyy_tjc_objTxt("app_url", earn_points_url);
    vTaskDelay(5 / portTICK_PERIOD_MS);
}

/**
 * @brief 
 * 操作控件
 * @param time 
 * @param next_status 
 */
void lyy_tjc_page_obj_pay_result(void)
{
    if (tjc_infor_t.point_config.point_enabled == 1) // 积分固件兼容
    {
        lyy_tjc_switch_page("result_p"); // 切换页面
    }
    else
    {
        lyy_tjc_switch_page("result"); // 切换页面
    }
    lyy_tjc_page_earn_points_url_show();
    lyy_tjc_device_infor();
    tourist_to_consume_add_points_show();
}

void lyy_tjc_page_scan_points_ndj(void)
{
#ifdef URL_TYPE_DEVELOP
    // 开发环境
#define DEV_URL_SCAN_POINTS_START "https://dc.cadmem.com/gw/customer/page/scan/"
#else
// 正式环境
#define DEV_URL_SCAN_POINTS_START "https://gc.starthing.com/gw/customer/page/scan/" //

#endif

#define DEV_URL_SCAN_POINTS_MIDDLE "?scene=marketPointAuthorize&uniqueCode="
#define DEV_URL_SCAN_POINTS_END "&commodityId="
    char scan_points_url[250] = {0};
    PAY_PARAM_DEF *para = get_pay_param();

    char id[6] = {0x00};
    vRandomGet(id, sizeof(id));
    memset(tjc_infor_t.unique_code, 0, sizeof(tjc_infor_t.unique_code));
    utils_lhexToString(tjc_infor_t.unique_code, (const unsigned char *)id, sizeof(id));

    // memcpy(tjc_infor_t.unique_code, get_random_string,6);
    sprintf(scan_points_url, "%s%s/%d%s%s%s%s", DEV_URL_SCAN_POINTS_START, APP_pcGetParamDevId(), (tjc_infor_t.package_record + 1), DEV_URL_SCAN_POINTS_MIDDLE, tjc_infor_t.unique_code, DEV_URL_SCAN_POINTS_END, para->ndj_pkgs.lane[tjc_infor_t.package_record].pkg.id);

    lyy_tjc_objTxt("app_pay_url", scan_points_url);
    vTaskDelay(5 / portTICK_PERIOD_MS);
}
void lyy_tjc_page_scan_points_std(void){

#ifdef URL_TYPE_DEVELOP
    // 开发环境
#define DEV_URL_SCAN_POINTS_START "https://dc.cadmem.com/gw/customer/page/scan/"
#else
    // 正式环境
    #define DEV_URL_SCAN_POINTS_START   "https://gc.starthing.com/gw/customer/page/scan/"  //

#endif

#define DEV_URL_SCAN_POINTS_MIDDLE  "?scene=marketPointAuthorize&uniqueCode="
#define DEV_URL_SCAN_POINTS_END     "&commodityId="
    char scan_points_url[250] = {0};
    PAY_PARAM_DEF *para = get_pay_param();

    char id[6] = {0x00};
    vRandomGet(id, sizeof(id));
    memset(tjc_infor_t.unique_code, 0, sizeof(tjc_infor_t.unique_code));
    utils_lhexToString(tjc_infor_t.unique_code, (const unsigned char *)id, sizeof(id));   

    //memcpy(tjc_infor_t.unique_code, get_random_string,6);
    sprintf(scan_points_url, "%s%s%s%s%s%s", DEV_URL_SCAN_POINTS_START, APP_pcGetParamDevId(), DEV_URL_SCAN_POINTS_MIDDLE,tjc_infor_t.unique_code, DEV_URL_SCAN_POINTS_END, para->pkgs.pkg[tjc_infor_t.package_record].id);

    lyy_tjc_objTxt("app_pay_url", scan_points_url);
    vTaskDelay(5 / portTICK_PERIOD_MS);
}

void lyy_tjc_page_scan_points(void)
{
    if (get_dev_type_code() == DEVICE_TYPE_PULSES) return;
    lyy_tjc_switch_page("scan_points");
    vTaskDelay(5 / portTICK_PERIOD_MS);

    lyy_tjc_device_infor();
    device_get_points_show();
    vTaskDelay(5 / portTICK_PERIOD_MS);
    points_deduction_max_amount_show();

    if (get_dev_type_code() == DEVICE_TYPE_UART_NDJ)
    {
        lyy_tjc_page_scan_points_ndj();
    }
    else
    {
        lyy_tjc_page_scan_points_std();
    }
}

void lyy_tjc_page_scan_points_success(void)
{
    lyy_tjc_switch_page("scan_points_t");
    vTaskDelay(5 / portTICK_PERIOD_MS);

    lyy_tjc_device_infor();
    member_to_consume_points_show();
}

void lyy_tjc_page_earn_points(void)
{
    lyy_tjc_switch_page("earn_points"); // 切换
    vTaskDelay(5 / portTICK_PERIOD_MS);


    lyy_tjc_device_infor();
    device_get_points_show();
    lyy_tjc_page_earn_points_url_show();

    //lyy_tjc_page_obj_regist(regist_url);
}

void lyy_tjc_page_earn_points_success(void)
{
    lyy_tjc_switch_page("earn_points_re");
    vTaskDelay(5 / portTICK_PERIOD_MS);

    lyy_tjc_device_infor();
    member_now_points_show();
}

void lyy_tjc_page_obj_pay_result_points(void)
{
    lyy_tjc_switch_page("result_points");
    vTaskDelay(5 / portTICK_PERIOD_MS);

    member_to_consume_add_points_show();

    lyy_tjc_device_infor();
}

/**
 * @brief 
 * taojingchi串口屏-显示pay结果页面
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
 * @brief 
 * 倒计时显示
 * @param time 
 * @param next_status 
 */
void lyy_tjc_count_down(uint16_t time,uint8_t next_status,uint8_t cur_status)
{
    tjc_infor_t.count_down.time = time;
    tjc_infor_t.count_down.next_status = next_status;

    if(cur_status != tjc_infor_t.status) return;

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
 * 显示提示信息
 * @param time 
 * @param context 
 */
void lyy_tjc_page_obj_amy_waring(char *context)
{
    char temp[10]={0};
    lyy_tjc_switch_page("waring");        //切换到首页
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    sprintf(temp," ");
    lyy_tjc_objTxt("time", temp);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_objTxt("body", context);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_device_infor();
}
/**
 * @brief 
 * 进入按摩椅倒计时界面
 * @param time 
 * @param context 
 */
void lyy_tjc_page_amy_working_V2(uint8_t statues,uint32_t time)
{
    // 空闲进入，切换页面
    if(statues == 0)
    {
        lyy_tjc_switch_page("count_down");        //切换到首页
        vTaskDelay(2 / portTICK_PERIOD_MS); 
    }

    // 显示倒计时时间 不进行倒计时
    char time_buf[30] = {0};

    lyy_tjc_device_infor();
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    amy_time_to_string(time,time_buf);
    vTaskDelay(2 / portTICK_PERIOD_MS); 
    lyy_tjc_obj_update_amy_count_down(time_buf);
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

    if(device_get_amy_err_Flag()) return;
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

    if(device_get_amy_err_Flag()) return;
    lyy_tjc_set_status(TJC_SHOW_PACKAGES);
}

/**
 * @brief
 * 点击获取积分
 * @param pack
 * @param len
 */
void lyy_tjc_touch_earn_points(char *pack, uint16_t len)
{
    (void)pack;
    (void)len;
    lyy_tjc_set_status(TJC_SHOW_EARN_POINTS);
}

/**
 * @brief
 * 点击正常套餐支付
 * @param pack
 * @param len
 */
void lyy_tjc_touch_pay_now(char *pack, uint16_t len)
{
    (void)pack;
    (void)len;

    lyy_tjc_packages_choose(tjc_infor_t.package_record, 0); // 默认第一个钱包 0
}

/**
 * @brief
 * 点击   会员正常套餐支付
 * @param pack
 * @param len
 */
void lyy_tjc_touch_pay_now_points(char *pack, uint16_t len)
{
    (void)pack;
    (void)len;
    tjc_infor_t.points_pay_flag = 2; // 标记积分抵扣登录会员，但是不使用积分消费
    // 里面需要区分扭蛋机和非扭蛋机
    lyy_tjc_packages_choose(tjc_infor_t.package_record, 0); // 默认第一个钱包 0


}

/**
 * @brief
 * 点击积分抵扣支付
 * @param pack
 * @param len
 */
void lyy_tjc_touch_pay_deduction(char *pack, uint16_t len)
{
    (void)pack;
    (void)len;

    tjc_infor_t.points_pay_flag = 1; // 标记积分抵扣消费

    // 积分抵扣消费  里面需要区分扭蛋机和非扭蛋机
    lyy_tjc_packages_choose(tjc_infor_t.package_record, 0); // 默认第一个钱包 0
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
 * @brief 
 * 
 * @param id  套餐id 从1开始
 * @param wallet_id 
 */
void lyy_tjc_get_pay_url_ndj(uint8_t id,uint8_t wallet_id)
{
    PAY_PARAM_DEF *para = get_pay_param();
    TJC_POINT_AUTHORIZE_DEF *point_authorize_data = get_point_authorize();
    cJSON_bool use_point_flag;


    char temp[64] = {0};
    LYY_LOGD("lyy_tjc_get_pay_url_ndj [package_id] :%d",id);
    LYY_LOGD("lyy_tjc_get_pay_url_ndj [wallet_id] :%d",wallet_id);   
    int ret = lyy_tjc_pay_id_isok(id, wallet_id);
    if(ret < 0 ) return ;
    if(strlen(para->ndj_pkgs.lane[id].pkg.id) == 0) //没有商品ID
    {
        //提示故障
        sprintf(temp,"pay packages[%d]: sku_id is null ",id);
        lyy_tjc_page_waring_normal(temp);
        return ;
    }
    uint8_t enable; //禁止则展示 “disable”
    
    enable = ndj_get_lane_enable(id);
    LYY_LOGD("package_id = %d ; enable = %d",id,enable);
    if(enable != 0x01)
    {
        sprintf(temp,"ndj lane[%d]: lane is disable ",id);
        lyy_tjc_page_waring_normal(temp);
        return ;
    }

    if (point_authorize_data->deduct_enable == 1) // 判断是否可以开启积分抵扣
    {
        use_point_flag = true;
    }
    else
    {
        use_point_flag = false;
    }

    if (tjc_infor_t.points_pay_flag == 1)  //积分抵扣消费
    {
        event_post_query_dynamic_url_ndj_points(
            para->ndj_pkgs.lane[id].pkg.id,          // 商品ID
            para->channel[wallet_id].code,           // 钱包编码
            para->channel[wallet_id].channel_number, // 渠道编号
            para->ndj_pkgs.lane[id].x,
            para->ndj_pkgs.lane[id].y,
            use_point_flag,
            point_authorize_data->auth_token,
            point_authorize_data->deduct_points,
            point_authorize_data->deduct_amount,
            point_authorize_data->pay_amount); // 获取动态支付二维码
    }
    else if (tjc_infor_t.points_pay_flag == 2)  //会员普通消费
    {
        event_post_query_dynamic_url_ndj_points(
            para->ndj_pkgs.lane[id].pkg.id,          // 商品ID
            para->channel[wallet_id].code,           // 钱包编码
            para->channel[wallet_id].channel_number, // 渠道编号
            para->ndj_pkgs.lane[id].x,
            para->ndj_pkgs.lane[id].y,
            false,
            point_authorize_data->auth_token,
            point_authorize_data->deduct_points,
            point_authorize_data->deduct_amount,
            point_authorize_data->pay_amount); // 获取动态支付二维码
    }
    else //游客消费
    {
        event_post_query_dynamic_url_ndj(
            para->ndj_pkgs.lane[id].pkg.id,          // 商品ID
            para->channel[wallet_id].code,           // 钱包编码
            para->channel[wallet_id].channel_number, // 渠道编号
            para->ndj_pkgs.lane[id].x,
            para->ndj_pkgs.lane[id].y); // 获取动态支付二维码
    }
    LYY_LOGD("region :%s",para->channel[wallet_id].region);
    LYY_LOGD("channelCode :%s",para->channel[wallet_id].channel_code);
    LYY_LOGD("wallet_code :%s",para->channel[wallet_id].code);
}
/**
 * @brief 
 * 
 * @param id  从1开始
 * @param wallet_id 
 */
void lyy_tjc_get_pay_url_std(uint8_t id,uint8_t wallet_id)
{
    PAY_PARAM_DEF *para = get_pay_param();
    TJC_POINT_AUTHORIZE_DEF *point_authorize_data = get_point_authorize();
    cJSON_bool use_point_flag;

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

    if (point_authorize_data->deduct_enable==1)  //判断是否可以开启积分抵扣
    {
        use_point_flag = true;
    }
    else
    {
        use_point_flag = false;
    }

    if (tjc_infor_t.points_pay_flag == 1) // 积分抵扣消费
    {
        event_post_query_dynamic_url_std_points(
            para->pkgs.pkg[id].id,                   // 商品ID
            para->channel[wallet_id].code,           // 钱包编码  //默认使用第一个钱包
            para->channel[wallet_id].channel_number, // 渠道编号  //默认使用第一个钱包
            use_point_flag,
            point_authorize_data->auth_token,
            point_authorize_data->deduct_points,
            point_authorize_data->deduct_amount,
            point_authorize_data->pay_amount); // 获取动态支付二维码
    }
    else if (tjc_infor_t.points_pay_flag == 2) // 会员普通消费
    {
        event_post_query_dynamic_url_std_points(
            para->pkgs.pkg[id].id,                  // 商品ID
            para->channel[wallet_id].code,          // 钱包编码  //默认使用第一个钱包
            para->channel[wallet_id].channel_number, // 渠道编号  //默认使用第一个钱包
            false,
            point_authorize_data->auth_token,
            point_authorize_data->deduct_points,
            point_authorize_data->deduct_amount,
            point_authorize_data->pay_amount); // 获取动态支付二维码
    }
    else // 游客消费
    {
        event_post_query_dynamic_url_std(
            para->pkgs.pkg[id].id,                  // 商品ID
            para->channel[wallet_id].code,          // 钱包编码  //默认使用第一个钱包
            para->channel[wallet_id].channel_number // 渠道编号  //默认使用第一个钱包
        );                                          // 获取动态支付二维码
    }
 
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
    if (get_dev_type_code() == DEVICE_TYPE_PULSES) return;  /* guest-only path below the UART dispatcher */
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
 * @brief Get the package id object
 * 
 * @param package 
 * @return int 
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

void lyy_tjc_points_page_skip(void)
{
    lyy_tjc_switch_page("scan_points"); // 切换页面


}

/**
 * @brief 
 * 点击套餐
 * @param pack 
 * @param len 
 */
void lyy_tjc_touch_package(char *pack,uint16_t len)
{
    if (get_dev_type_code() == DEVICE_TYPE_PULSES) return;

    (void)pack;  
    (void)len;

    tjc_infor_t.package_record = get_package_id(pack);
    if (tjc_infor_t.point_config.point_enabled==1) //是否开启积分消费 是进入积分消费模式
    {
        lyy_tjc_set_status(TJC_SHOW_SCAN_POINTS);
    }
    else
    {
        lyy_tjc_packages_choose(get_package_id(pack), 0); // 默认第一个钱包 0  
    }
    
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
    if (get_dev_type_code() == DEVICE_TYPE_PULSES) {
        /* Match the existing dispatcher even when the frame has extra bytes. */
        for (uint8_t i = 0; i < PAY_CHANNEL_MAX; ++i) {
            char cmd[] = "wallet0";
            cmd[6] += i;
            if (strnstr(pack, cmd, len)) {
                TJC_PULSES_TOUCH touch = { .kind = PULSES_TOUCH_WALLET, .index = i };
                if (pulses_touches) xQueueSend(pulses_touches, &touch, 0);
                break;
            }
        }
        return;
    }

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

static int lyy_tjc_screen_version_format(char *dst, size_t dst_size, const char *pack, uint16_t len)
{
    char raw[sizeof(tjc_infor_t.tjc_ver)] = {0};
    uint16_t copy_len = 0;
    const char *ver = NULL;
    const char *p = NULL;

    if ((dst == NULL) || (dst_size == 0) || (pack == NULL) || (len == 0))
    {
        return 0;
    }

    for (uint16_t i = 0; (i < len) && (copy_len < sizeof(raw) - 1); i++)
    {
        uint8_t ch = (uint8_t)pack[i];

        if ((ch == 0x00) || (ch == 0xFF) || (ch == '\r') || (ch == '\n'))
        {
            break;
        }

        raw[copy_len++] = (char)ch;
    }

    if (copy_len <= 1)
    {
        return 0;
    }

    ver = raw;
    p = strchr(ver, ':');
    if (p != NULL)
    {
        ver = p + 1;
    }

    // p = strchr(ver, '_');
    // if ((strncmp(ver, "TJC", 3) == 0) && (p != NULL) && (*(p + 1) != '\0'))
    // {
    //     ver = p + 1;
    // }

    if (strlen(ver) <= 1)
    {
        return 0;
    }

    memset(dst, 0, dst_size);
    strncpy(dst, ver, dst_size - 1);
    return 1;
}

/**
 * @brief 
 * 定时发送版本
 * @param pack 
 * @param len 
 */
void lyy_tjc_time_version(char *pack,uint16_t len)
{

    (void)len;

    LYY_LOGD("ver:%s",pack);
    // char ver_screen[50] = {0};
    // char *p = NULL;

    // 获取屏幕型号
    if((strstr(pack,LYY_SCREEN_DEVICE_TJC8048x270) != NULL) || (strstr(pack,LYY_SCREEN_DEVICE_TJC8048x243) != NULL) )
    {
        set_tjcScreen(LYY_SCREEN_DEVICE);
        // 获取屏幕方向
        if(strstr(pack,"_00_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_H);
        }
        else if(strstr(pack,"_270_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_V);
        }
    }
    else if(strstr(pack,LYY_SCREEN_DEVICE_TJC4880x243) != NULL) 
    {
        set_tjcScreen(LYY_SCREEN_DEVICE_4880x243);
        // 获取屏幕方向
        if(strstr(pack,"_00_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_00);
        }
        else if(strstr(pack,"_90_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_90);
        }else if(strstr(pack,"_180_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_180);
        }
        else if(strstr(pack,"_270_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_270);
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
    if(strlen(pack)>1&&strlen(pack)<100)
    {
        // memset(tjc_infor_t.tjc_ver,0,sizeof(tjc_infor_t.tjc_ver));
        // strcpy(tjc_infor_t.tjc_ver,pack);
        lyy_tjc_screen_version_format(tjc_infor_t.tjc_ver, sizeof(tjc_infor_t.tjc_ver), pack, len);
        LYY_LOGD(" tjc_ver : %s", tjc_infor_t.tjc_ver);
    }    
    lyy_tjc_time_en("tm0", 0);
}

/**
 * @brief 
 * 
 * @param pack 
 * @param len 
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
        set_tjcScreen(LYY_SCREEN_DEVICE);
        // 获取屏幕方向
        if(strstr(pack,"_00_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_H);
        }
        else if(strstr(pack,"_270_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_V);
        }
    }
    else if(strstr(pack,LYY_SCREEN_DEVICE_TJC4880x243) != NULL) 
    {
        set_tjcScreen(LYY_SCREEN_DEVICE_4880x243);
        // 获取屏幕方向
        if(strstr(pack,"_00_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_00);
        }
        else if(strstr(pack,"_90_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_90);
        }else if(strstr(pack,"_180_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_180);
        }
        else if(strstr(pack,"_270_") != NULL) 
        {
            set_tjcScreenType(LYY_SCREEN_TYPC_270);
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
    if (strlen(pack) > 1 && strlen(pack) < 100)
    {
        // memset(tjc_infor_t.tjc_ver,0,sizeof(tjc_infor_t.tjc_ver));
        // strcpy(tjc_infor_t.tjc_ver,pack);
        lyy_tjc_screen_version_format(tjc_infor_t.tjc_ver, sizeof(tjc_infor_t.tjc_ver), pack, len);
        LYY_LOGD(" tjc_ver : %s", tjc_infor_t.tjc_ver);
    }
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


/* Touch text may be raw or wrapped in the existing TJC string-return frame. */
static int tjc_pulses_recv_touch(const uint8_t *pack, uint32_t len)
{
    TJC_PULSES_TOUCH touch = {0};
    if (len >= 3 && memcmp(pack + len - 3, "\xff\xff\xff", 3) == 0)
    {
        len -= 3;
        if (len >= 2 && pack[0] == 0x70 && (pack[1] == 'p' || pack[1] == 'w'))
        {
            ++pack;
            --len;
        }
    }
    if (len == 9 && memcmp(pack, "position", 8) == 0 &&
        pack[8] >= '1' && pack[8] < '1' + PULSES_LANE_COUNT)
    {
        touch.kind = PULSES_TOUCH_POSITION;
        touch.index = pack[8] - '0';
    }
    else if (len == 8 && memcmp(pack, "package", 7) == 0 &&
             pack[7] >= '1' && pack[7] < '1' + PULSES_PACKAGE_MAX)
    {
        touch.kind = PULSES_TOUCH_PACKAGE;
        touch.index = pack[7] - '1';
    }
    else
        return 0;

    touch.generation = __atomic_load_n(&pulses_generation, __ATOMIC_RELAXED);
    if (pulses_touches != NULL) xQueueSend(pulses_touches, &touch, 0);
    return 1;
}

/**
 * @brief 
 * 
 * @param pack 
 * @param len 
 */
void tjc_recv_packet_handle(uint8_t *pack,uint32_t len)
{

#if 1

    uint8_t i = 0;
    
    //lyy_display_recvPic(pack,len);

    if (board_ota_param_t.status >= SCREEN_OTA_BAUD_SET) // 屏幕升级响应ACK
    {
        lyy_printf_hex(pack, len);
        if (pack[0] == 0x05) // 传输成功
        {
            lyy_semSignalV2(screen_ota_ota_cmd_send); // 继续传输
        }
        else if (pack[0] == 0x1A) // 设置成功
        {
            lyy_semSignalV2(screen_ota_baud_set); // 设置成功
        }
        else if (strstr((char *)pack, "comok")) // 连接成功
        {
            lyy_semSignalV2(screen_ota_connect_cmd_send); // 连接成功
        }
    }
    else
    {
        lyy_display_recvPic(pack, len);
        if (get_dev_type_code() == DEVICE_TYPE_PULSES && tjc_pulses_recv_touch(pack, len)) return;
    }

#if 0
    uint8_t packetlen = 0;
    char packet[254] = {0};
    memset(packet,0,sizeof(packet));

    if(tjc_isPacket((uint8_t *)pack, len) > 0)
    {
        LYY_LOGD("packet[0]:%02x",pack[0]);
        packetlen = len - 4;
        packetlen = packetlen > sizeof(packet) ? sizeof(packet)-1 : packetlen;

        if(packetlen>0)  
            memcpy(packet,pack+1,packetlen);  
        else 
            return ; //继续解析

    }
    else
    {
        packetlen = len;
        packetlen = packetlen>sizeof(packet) ? sizeof(packet)-1 : packetlen;
        memcpy(packet,pack,packetlen);    
    }
#endif

    for(i=0;i<(sizeof(tjc_packet_parse_t)/sizeof(tjc_packet_parse_t[0]));i++)
    {
        if(strnstr((char *)pack,(char *)tjc_packet_parse_t[i].cmd, len) != NULL)
        {
            if(tjc_packet_parse_t[i].parse != NULL)
                tjc_packet_parse_t[i].parse((char *)pack,len);
            break;
        }
    }

#endif

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
        g_tAMYState.OffStatusFlag = 0;       
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
 * @brief 
 * 
 */
void lyy_tjc_page_change_amy_time(void)
{
    char time[50] = {0};
    amy_time_t.sys_tick_start = HAL_ulGetSysTick()/1000;   
    lyy_tjc_switch_page("count_down");
    amy_time_to_string(tjc_infor_t.amy_time.total,time);
    lyy_tjc_page_obj_count_down("Count Down",time); 
    tjc_infor_t.status = TJC_SHOW_AMY_TIME_WORK;   
}
/**
 * @brief 
 * 
 */
void lyy_tjc_page_amy_time_work(void)
{
    char time[50] = {0};
    vTaskDelay(100 / portTICK_PERIOD_MS); 
    amy_time_t.sys_tick_last = HAL_ulGetSysTick()/1000;
    amy_time_t.sys_tick_value = (amy_time_t.sys_tick_last - amy_time_t.sys_tick_start);  //系统tick 20ms间隔
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
 * @brief 
 * 
 * @return int 
 */
int amy_time_redisplay(void)
{
    if(g_tAMYState.OffStatusFlag != 0)
    {
        lyy_tjc_set_status(TJC_SHOW_AMY_TIME_WORK_V2);
        //lyy_tjc_switch_page("count_down");        //切换到首页
        lyy_tjc_page_amy_working_V2(0,tjc_infor_t.amy_time.total);
        return true;
    }

    char time[50] = {0};
    if(amy_time_t.work_value >=4 && tjc_infor_t.amy_time.total != 0) //需要显示
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
 * 发送连接指令 -- 屏幕升级
 * @param file_name
 * @param sum
 * @param num
 * @param data_len
 * @param context
 */
void lyy_tjc_baud_set(int baud)
{
    char *dataBuf = NULL;
    dataBuf = malloc(100);
    if (dataBuf == NULL)
    {
        LYY_LOGD("mallco is fail");
        return;
    }

    memset(dataBuf, 0, 100);
    sprintf(dataBuf, "baud=%d\xff\xff\xff", baud);
    LYY_LOGD("lyy_tjc_cmd_send>%s", dataBuf);
    lyy_tjc_cmd_send(dataBuf, strlen(dataBuf));
    free(dataBuf);
}

/**
 * @brief
 * 发送连接指令 -- 屏幕升级
 * @param file_name
 * @param sum
 * @param num
 * @param data_len
 * @param context
 */
void lyy_tjc_connect_cmd_send(void)
{
    char pattern[] = {0x00, 0xff, 0xff, 0xff, 'c', 'o', 'n', 'n', 'e', 'c', 't', 0xff, 0xff, 0xff};
    //LYY_LOGD("lyy_tjc_cmd_send>%s", pattern);
    //LYY_LOGD_hex(pattern, sizeof(pattern));
    lyy_tjc_cmd_send(pattern, sizeof(pattern));
}

/**
 * @brief
 * 发送下载指令 -- 屏幕升级
 * @param file_name
 * @param sum
 * @param num
 * @param data_len
 * @param context
 */
// void lyy_tjc_ota_cmd_send(uint32_t file_size, uint32_t baud)
// {
//     char *dataBuf = NULL;
//     dataBuf = malloc(100);
//     if (dataBuf == NULL)
//     {
//         LYY_LOGD("mallco is fail");
//         return;
//     }

//     memset(dataBuf, 0, 100);
//     LYY_LOGD("file_size = %d baud = %d", file_size, baud);
//     sprintf(dataBuf, "whmi-wri %d,%d,0\xff\xff\xff", (int)file_size, (int)baud);
//     LYY_LOGD("dataBuf =test");
//     lyy_tjc_cmd_send(dataBuf, strlen(dataBuf));
//     LYY_LOGD("dataBuf =test2222");
//     free(dataBuf);
// }

#define TJC_OTA_BUF_LEN 100
void lyy_tjc_ota_cmd_send(uint32_t file_size, uint32_t baud)
{
    char *dataBuf = NULL;
    dataBuf = malloc(TJC_OTA_BUF_LEN);
    if (dataBuf == NULL)
    {
        LYY_LOGD("mallco is fail");
        return;
    }

    memset(dataBuf, 0, TJC_OTA_BUF_LEN);

    LYY_LOGD("file_size = %lu baud = %lu", file_size, baud);
    // snprintf获取真实发送长度，避免丢失尾部0xff
    int send_len = snprintf(dataBuf, TJC_OTA_BUF_LEN,
                             "whmi-wri %lu,%lu,0\xff\xff\xff",
                             file_size, baud);
    if (send_len <= 0 || send_len >= TJC_OTA_BUF_LEN)
    {
        LYY_LOGE("ota cmd format length error, len:%d", send_len);
        free(dataBuf);
        return;
    }

    LYY_LOGD("send_len =%d", send_len);
    lyy_tjc_cmd_send(dataBuf, send_len);
    free(dataBuf);
}

/**
 * @brief
 * 发送数据包 -- 屏幕升级
 * @param file_name
 * @param sum
 * @param num
 * @param data_len
 * @param context
 */
void lyy_tjc_ota_data_send(uint16_t len, uint8_t *context)
{
    lyy_tjc_cmd_send((char *)context, len);
}

/**
 * @brief
 * 发送屏幕ota数据包
 */
int screen_ota_status_send(void)
{
    uint32_t offset = 0;
    uint8_t *buffer = NULL;
    uint32_t len;
    int8_t ret;
    uint16_t read_size;
    //uint16_t size = board_ota_param_t.file_size >= 0xffff ? 0xffff : board_ota_param_t.file_size;
    LYY_LOGD("file_name = %s  para->file_size = %d", THING_IOT_SCREEN_PATH, board_ota_param_t.file_size);
    buffer = malloc(4096);
    if (buffer == NULL)
    {
        LYY_LOGD("mallco is fail");
        return -1;
    }
    for (uint16_t i = 1; i <= board_ota_param_t.sum; i++)
    {
        len = board_ota_param_t.file_size - offset;
        len = (len > 4096) ? 4096 : len;
        read_size = HAL_vFileReadOffset(THING_IOT_SCREEN_PATH, buffer, len, offset);
        if (read_size != len)
        {
            LYY_LOGD("board read file fail");
            return -1;
        }

        offset += len; // 中间帧
        //lyy_printf_hex(buffer, 512);
        lyy_semNewV2(&screen_ota_ota_cmd_send, 0);
        lyy_tjc_ota_data_send(len, buffer);
        ret = lyy_semWaitTryV2(screen_ota_ota_cmd_send, 1000);
        lyy_semFreeV2(screen_ota_ota_cmd_send);
        screen_ota_ota_cmd_send = NULL;
        vTaskDelay(10 / portTICK_PERIOD_MS); // 延时读取文件
        if (ret == 0)
            return -1;
    }
    return 1;
}
/**
 * @brief 
 * 显示屏业务
 */
void thread_screen_ota(void *arg)
{

    (void)arg;
    int8_t ret;
    uint8_t baudIndex = 0;
    uint8_t i = 0;
    while(1)
    {
        switch (board_ota_param_t.status)
        {
        case BOARD_OTA_STATUS_IDLE:
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            break;                       
        case BOARD_OTA_STATUS_DATA_SEND: // 发送数据
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            break;                                            // 发送数据
        case SCREEN_OTA_BAUD_SET:
        {
            LYY_LOGD("os_uart_open ");

            HAL_vUartSetParam(HAL_UART_SCREEN, baudTable[baudIndex]);

            vTaskDelay(200 / portTICK_PERIOD_MS);
            LYY_LOGD("baudset %d", baudTable[baudIndex]);
            lyy_tjc_baud_set(baudTable[baudIndex]);
            baudIndex++;
            if (baudIndex > 2)
            {
                board_ota_param_t.status = BOARD_OTA_STATUS_IDLE; // 空闲
                LYY_LOGD("BOARD_OTA_STATUS_IDLE >>>>>");
                HAL_vFileClear(THING_IOT_SCREEN_PATH);
                baudIndex = 0;
                break;
            }
            LYY_LOGD("SCREEN_OTA_STATUS_INIT >>>>>");
            board_ota_param_t.status = SCREEN_OTA_STATUS_INIT;
            vTaskDelay(200 / portTICK_PERIOD_MS);
        }
        break;
        case SCREEN_OTA_STATUS_INIT: // 发送下载指令

            tjc_infor_t.step = SCREEN_START_STEP_OTA;
            tjc_infor_t.status = TJC_SHOW_SCREEN_OTA;

            for (i = 0; i < 3; i++)
            {
                lyy_semNewV2(&screen_ota_connect_cmd_send, 0);
                lyy_tjc_connect_cmd_send();
                ret = lyy_semWaitTryV2(screen_ota_connect_cmd_send, 500);
                lyy_semFreeV2(screen_ota_connect_cmd_send);
                screen_ota_connect_cmd_send = NULL;
                if (ret > 0)
                    break;
            }
            if (ret > 0) // 设置成功
            {
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                LYY_LOGD("baudset %d", baudTable[baudIndex]);
                lyy_semNewV2(&screen_ota_baud_set, 0);
                lyy_tjc_baud_set(921600); // 921600
                ret = lyy_semWaitTryV2(screen_ota_baud_set, 500);
                lyy_semFreeV2(screen_ota_baud_set);
                screen_ota_baud_set = NULL;

                HAL_vUartSetParam(HAL_UART_SCREEN, 921600);

                vTaskDelay(2000 / portTICK_PERIOD_MS);
                LYY_LOGD("file_size = %d baud = %d", board_ota_param_t.file_size, 921600);
                lyy_semNewV2(&screen_ota_ota_cmd_send, 0);
                lyy_tjc_ota_cmd_send(board_ota_param_t.file_size, 921600);
                ret = lyy_semWaitTryV2(screen_ota_ota_cmd_send, 500);
                lyy_semFreeV2(screen_ota_ota_cmd_send);
                screen_ota_ota_cmd_send = NULL;
                if (ret > 0) // 成功
                {
                    LYY_LOGD("SCREEN_OTA_STATUS_DATA_SEND >>>>>");
                    board_ota_param_t.status = SCREEN_OTA_STATUS_DATA_SEND; //
                }
                else // ota 启动指令失败，回退换波特率重试
                {
                    LYY_LOGD("ota_cmd fail >> SCREEN_OTA_BAUD_SET >>>>>");
                    board_ota_param_t.status = SCREEN_OTA_BAUD_SET;
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                }
            }
            else
            { // 切换波特率
                LYY_LOGD("change baud >>  SCREEN_OTA_BAUD_SET >>>>>");
                board_ota_param_t.status = SCREEN_OTA_BAUD_SET;
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }
            break;
        case SCREEN_OTA_STATUS_DATA_SEND: // 发送数据
            LYY_LOGD("start screen ota >>>>>>>>>");
            ret = screen_ota_status_send();
            LYY_LOGD("screen ota finished ret = %d | > 0 success", ret);
            if (ret > 0) // 升级成功：上报版本、清文件、重启
            {

                vTaskDelay(5000 / portTICK_PERIOD_MS);
                //lyy_tjc_screen_type_get();
                vTaskDelay(500 / portTICK_PERIOD_MS);
                //thing_ota_inform_version(tjc_infor_t.tjc_ver, THING_OTA_SCREEN); // 上报屏幕版本信息

                HAL_vFileClear(THING_IOT_SCREEN_PATH);
                // 屏幕升级完成 重启盒子
                HAL_vReset("screen ota", 1000);
            }
            else // 升级失败：不清文件、不重启，回到 INIT 重新握手并重发
            {
                LYY_LOGE("### screen ota send fail, retry from INIT");
                board_ota_param_t.status = SCREEN_OTA_STATUS_INIT;
                vTaskDelay(500 / portTICK_PERIOD_MS);
            }

            break; // 发送数据
        default:
            vTaskDelay(5000 / portTICK_PERIOD_MS);
            break;
        }

    }
}


static int tjc_pulses_request(const TJC_PULSES_SLOT *slot, uint8_t position, uint8_t wallet)
{
    uint8_t status = tjc_infor_t.status;
    PAY_PARAM_DEF *para = get_pay_param();
    uint8_t wallet_sum = para->chnl_sum;
    if (wallet_sum > PAY_CHANNEL_MAX || wallet >= wallet_sum ||
        para->channel[wallet].code[0] == '\0' || para->channel[wallet].channel_number[0] == '\0')
        return 0;

    static uint16_t sequence;
    char token[sizeof(pulses_qr_token)];
    snprintf(token, sizeof(token), "%08lx%04x", (unsigned long)HAL_ulGetRandom(), (unsigned int)++sequence);
    taskENTER_CRITICAL();
    xQueueReset(pulses_qr_replies);
    memcpy(pulses_qr_token, token, sizeof(token));
    taskEXIT_CRITICAL();
    LYY_LOGD("pulses get_qrcode: lane=%u wallet=%u package=%s", position, wallet, slot->id);
    if (event_post_query_dynamic_url_pulses(slot->id, para->channel[wallet].code,
            para->channel[wallet].channel_number, position, token) != 1) {
        taskENTER_CRITICAL();
        pulses_qr_token[0] = '\0';
        taskEXIT_CRITICAL();
        return 0;
    }
    if (tjc_infor_t.status != status) {
        taskENTER_CRITICAL();
        pulses_qr_token[0] = '\0';
        taskEXIT_CRITICAL();
        return 1; /* Do not reopen a page the user left during send. */
    }

    pulses_payment.slot = *slot;
    pulses_payment.position = position;
    pulses_payment.pending = 1;
    pulses_on_packages = 0;
    tjc_infor_t.pay.wallet_num = wallet;
    tjc_infor_t.pay.wallet_sum = wallet_sum;
    tjc_infor_t.pay.time = PAY_COUNTDOWN_MAX;
    lyy_tjc_set_status(TJC_SHOW_PAY_PAGE);
    return 1;
}

static void tjc_pulses_poll(void)
{
    uint8_t status = tjc_infor_t.status;
    if (status != TJC_SHOW_UPDATE_NET) pulses_on_packages = 0;
    if (status != TJC_SHOW_PAY_PAGE && status != TJC_SHOW_COUNT_DOWN && status != TJC_SHOW_PAY_URL)
    {
        if (pulses_payment.position != 0) {
            taskENTER_CRITICAL();
            pulses_qr_token[0] = '\0';
            taskEXIT_CRITICAL();
        }
        pulses_payment.position = 0;
        pulses_payment.pending = 0;
    }
    if (pulses_touches == NULL || pulses_qr_replies == NULL) return;

    if (status == TJC_SHOW_COUNT_DOWN && pulses_payment.pending) {
        taskENTER_CRITICAL();
        int ready = xQueueReceive(pulses_qr_replies, &pulses_payment.reply, 0) == pdPASS;
        if (ready) pulses_qr_token[0] = '\0';
        taskEXIT_CRITICAL();
        if (ready) {
            pulses_payment.pending = 0;
            lyy_tjc_set_status(TJC_SHOW_PAY_URL);
            return;
        }
    }

    TJC_PULSES_TOUCH touch;
    if (xQueueReceive(pulses_touches, &touch, 0) == pdPASS)
    {
        if (pulses_on_packages && status == TJC_SHOW_UPDATE_NET)
        {
            if (touch.kind == PULSES_TOUCH_POSITION && touch.index != pulses_position)
            {
                pulses_position = touch.index;
                tjc_pulses_render();
                return;
            }
            if (touch.kind == PULSES_TOUCH_PACKAGE && pulses_slots[touch.index].id[0] &&
                touch.generation == __atomic_load_n(&pulses_generation, __ATOMIC_RELAXED) &&
                pulses_revision == pulses_get_revision())
            {
                if (!tjc_pulses_request(&pulses_slots[touch.index], pulses_position, 0))
                    lyy_tjc_page_waring_normal("Payment request unavailable");
                return;
            }
        }
        else if (touch.kind == PULSES_TOUCH_WALLET && status == TJC_SHOW_COUNT_DOWN &&
                 pulses_payment.position != 0 && touch.index < tjc_infor_t.pay.wallet_sum &&
                 touch.index != tjc_infor_t.pay.wallet_num)
        {
            if (!tjc_pulses_request(&pulses_payment.slot, pulses_payment.position, touch.index))
                lyy_tjc_page_waring_normal("Payment request unavailable");
        }
    }

    if (pulses_on_packages && tjc_infor_t.status == TJC_SHOW_UPDATE_NET &&
        pulses_revision != pulses_get_revision())
        tjc_pulses_render();
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

    device_get_amy_err_powerOn();       // 按摩椅特殊操作，如果有故障，显示故障

    while(1)
    {
        if (get_dev_type_code() == DEVICE_TYPE_PULSES) tjc_pulses_poll();
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
                tjc_infor_t.points_pay_flag = 0; //清空积分抵扣消费标记

                lyy_tjc_packages_show();
                if (get_dev_type_code() != DEVICE_TYPE_PULSES) device_get_points_show();
                memset(&tjc_infor_t.count_down,0x00,sizeof(TJC_COUNT_DOWN_DEF));
                if (get_dev_type_code() != DEVICE_TYPE_PULSES || tjc_infor_t.status == TJC_SHOW_PACKAGES)
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
                lyy_tjc_count_down(tjc_infor_t.pay.time,TJC_SHOW_PACKAGES, TJC_SHOW_PAY_URL);
            }break; 
            case TJC_SHOW_PAY_PAGE: //支付界面
            {
                lyy_tjc_change_pay_page(tjc_infor_t.pay.wallet_sum,tjc_infor_t.pay.wallet_num);
                lyy_tjc_count_down(tjc_infor_t.pay.time,TJC_SHOW_PACKAGES, TJC_SHOW_PAY_PAGE);
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
                        if (get_dev_type_code() == DEVICE_TYPE_PULSES && pulses_payment.pending) {
                            LYY_LOGD("pulses get_qrcode timeout: lane=%u wallet=%u",
                                     pulses_payment.position, tjc_infor_t.pay.wallet_num);
                        }
                        tjc_infor_t.status = tjc_infor_t.count_down.next_status;
                    } 
                    lyy_tjc_net_status();         
                }
                vTaskDelay(20 / portTICK_PERIOD_MS);
            }break;   
            case  TJC_SHOW_WARING: //提示界面
            {
                lyy_tjc_page_obj_waring(tjc_infor_t.waring.context,tjc_infor_t.waring.time);
                lyy_tjc_count_down(tjc_infor_t.waring.time,tjc_infor_t.waring.next_status,TJC_SHOW_WARING);
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
                lyy_tjc_count_down(tjc_infor_t.success.time,tjc_infor_t.success.next_status,TJC_SHOW_PAY_RESULT);    
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
            case TJC_SHOW_AMY_TIME_WORK_V2: // 按摩椅上报运行显示
            {
                vTaskDelay(20 / portTICK_PERIOD_MS); 
            }break;
            case TJC_SHOW_SCAN_POINTS: // 扫描抵扣积分
            {
                lyy_tjc_page_scan_points();
                lyy_tjc_count_down(PAY_COUNTDOWN_MAX, TJC_SHOW_PACKAGES, TJC_SHOW_SCAN_POINTS);
            }
            break;
            case TJC_SHOW_SCAN_POINTS_SUCCESS: // 扫描抵扣积分,登录成功
            {
                lyy_tjc_page_scan_points_success();
                lyy_tjc_count_down(PAY_COUNTDOWN_MAX, TJC_SHOW_PACKAGES, TJC_SHOW_SCAN_POINTS_SUCCESS);
            }
            break;
            case TJC_SHOW_EARN_POINTS: // 获取积分页面
            {
                lyy_tjc_page_earn_points();
                lyy_tjc_count_down(PAY_COUNTDOWN_MAX, TJC_SHOW_PACKAGES, TJC_SHOW_EARN_POINTS);
            }
            break;
            case TJC_SHOW_EARN_POINTS_SUCCESS: // 获取积分成功
            {
                lyy_tjc_page_earn_points_success();
                lyy_tjc_count_down(PAY_COUNTDOWN_MAX, TJC_SHOW_PACKAGES, TJC_SHOW_EARN_POINTS_SUCCESS);
            }
            break;
            case TJC_SHOW_PAY_RESULT_POINTS: // 积分抵扣付款成功
            {
                lyy_tjc_page_obj_pay_result_points();
                lyy_tjc_count_down(10, TJC_SHOW_PACKAGES, TJC_SHOW_PAY_RESULT_POINTS);
            }
            break;
            case TJC_SHOW_SCREEN_OTA:
            {
                vTaskDelay(4000 / portTICK_PERIOD_MS);
            }
            break;
            default: //未知状态 显示套餐
            {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                tjc_infor_t.status = TJC_SHOW_PACKAGES;
            }break;
        }
    }
}


void thread_dowload_semGive(void)
{
    if(g_SemSta){

        xSemaphoreGive(hw_httpDowloadSem);  
        g_SemSta = 0x00;
    }
}

/**
 * @brief 
 * 显示屏业务
 */
void thread_screen_init(void *arg)
{

    (void)arg;
    int ret = 0;

    lyy_picInit();

    hw_httpDowloadSem = xSemaphoreCreateBinary(); 
    
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

#define TJC_DOWLOAD_THREAD_NAME             "TJC_DOWNLOAD_THREAD"
#define TJC_DOWLOAD_THREAD_STACK            (1024 * 8)


                    xTaskCreate(    thread_tjcPicDealThread, 
                                    TJC_DOWLOAD_THREAD_NAME, 
                                    TJC_DOWLOAD_THREAD_STACK, 
                                    NULL, 
                                    5,
                                    NULL); 

                    g_SemSta = 1;
                    int result = xSemaphoreTake(hw_httpDowloadSem, 5*60000);           
                    g_SemSta = 0;

                    if(result > 0) //成功
                    {
                        lyy_tjc_page_waring_init(lyy_tjc_img_result(tjc_infor_t.img_result),4); 
                    }
                    else
                    {
                        lyy_tjc_page_waring_init("Image updates :  timeout",4);
                    }
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
            case SCREEN_START_STEP_OTA:
            {
                vTaskDelay(4000 / portTICK_PERIOD_MS);
            }
            break;
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
 * @brief 
 * taojingchi串口屏始化
 */
void lyy_init_tjc(void)
{

    tjc_infor_t.step = SCREEN_START_STEP_HOMEPAGE;
    //strcpy(tjc_infor_t.tjc_ver,"tjc_default_v3.0.0");
    
#define SCAN_UART_BRT               115200
#define TJC_THREAD_NAME             "TJC_THREAD"
#define TJC_THREAD_STACK            (1024 * 4 / sizeof(portSTACK_TYPE))


    if (get_dev_type_code() == DEVICE_TYPE_PULSES)
    {
        pulses_touches = xQueueCreate(8, sizeof(TJC_PULSES_TOUCH));
        pulses_qr_replies = xQueueCreate(1, sizeof(TJC_PULSES_QR));
        if (pulses_touches == NULL || pulses_qr_replies == NULL)
            LYY_LOGE("TJC pulses: could not allocate screen queues");
    }

    HAL_vUartInit(HAL_UART_SCREEN,      SCAN_UART_BRT);
    HAL_vUartRecvProc(HAL_UART_SCREEN,  tjc_recv_packet_handle); 

    xTaskCreate(    thread_screen_init, 
                    TJC_THREAD_NAME, 
                    TJC_THREAD_STACK, 
                    NULL, 
                    5,
                    NULL);

    xTaskCreate(thread_screen_ota,
                "TJC_OTA_THREAD",
                TJC_THREAD_STACK,
                NULL,
                5,
                NULL);
}