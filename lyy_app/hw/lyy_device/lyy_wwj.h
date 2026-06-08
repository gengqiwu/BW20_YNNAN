/*
 * @author: e.he
 * @Date: 2024-07-10 16:30:26
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-08-09 18:05:11
 * @Description: 
 */
#ifndef __LYY_WWJ_H__
#define __LYY_WWJ_H__


//娃娃机主动上传账目
typedef struct
{
    uint8_t mark;       //查询主板链接类型
    uint8_t fault;      //上分标识
    uint8_t coin;
    uint8_t gift;
    uint8_t note;
    uint8_t res[2];
}__attribute__((aligned(1), packed))DEVICE_ACC_WWJ_DEF,*PDEVICE_ACC_WWJ_DEF;
//对应04指令
typedef struct
{
    uint8_t    err;         //错误码
    uint32_t   coin;        //投币
    uint32_t   gift;        //出礼
    uint32_t   note;        //纸币
    uint32_t   cpay;        //云投币
}__attribute__((aligned(1), packed))DEVICE_ACC_WWJ_T,*PDEVICE_ACC_WWJ_T;


extern int device_wwj_pay_apply(void);
extern int device_wwj_pay_start(uint16_t coin,uint16_t money);
extern void device_wwj_accounts_update(uint8_t *package, uint16_t len);

#endif



