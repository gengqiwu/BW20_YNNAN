/*
 * @author: e.he
 * @Date: 2024-06-20 17:42:12
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-08-07 15:04:45
 * @Description: 
 */
#ifndef __THING_PROPERTY_H__
#define __THING_PROPERTY_H__



#define FAULT_MSG_MAX           16

//属性
typedef struct
{
    char     method[24]; //
    int      (*pro_write)(cJSON *body);
    int      (*pro_read)(cJSON *body);
}PROPER_METHOD_DEF;


/**
 * @brief 
 * 故障信息
 */
typedef struct
{
    int         errCode;         //故障代码
    char        errMsg[64];      //文本信息    
    uint8_t     errFlag;         //等级，是否报故障
}__attribute__((aligned(1), packed))FAULT,*PFAULT;

/**
 * @brief 
 * 
 */
typedef struct
{
    uint8_t  num;
    uint16_t code;
    char language[32];
    FAULT fault_t[FAULT_MSG_MAX];
}__attribute__((aligned(1), packed))DEV_FAULT,*PDEV_FAULT;

/**
 * @brief 
 * 登录标识
 */
typedef struct
{
    uint16_t login;
    uint8_t  result;
    char msg[32];
}__attribute__((aligned(1), packed))DEV_LOGIN_INFOR_DEF;
extern  DEV_LOGIN_INFOR_DEF  dev_login_infor_t;


extern void property_post_dev_infor(void);
extern void property_post_net_infor(void);
extern void property_post_uart_brt_infor(void);
extern void thing_property_set(cJSON* params);
extern void thing_property_read(cJSON* params);

extern void device_login_infor_update(uint16_t login,uint8_t flag,char *msg);
extern void property_post_login_flag(uint16_t login_flag,uint8_t result,char *message);
extern void pay_package_update(void *package,cJSON *body);

extern void property_post_csq_level(void);
extern void property_post_lbs_cell(void *para);
extern void property_post_wifi_info(void);
extern char *get_fault_msg(uint16_t code);
extern void dev_fault_init(void);
extern DEV_FAULT *get_dev_fault_para(void);

#endif // 




