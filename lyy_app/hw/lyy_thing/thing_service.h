/*
 * @author: e.he
 * @Date: 2024-06-20 17:20:57
 * @LastEditors: Please set LastEditors
 * @LastEditTime: 2024-07-02 14:31:04
 * @Description: 
 */
#ifndef  __THING_SERVICE_H__
#define  __THING_SERVICE_H__

#include "cJSON.h"

//服务
typedef struct
{
    char     method[32]; //
    int      (*parse)(cJSON *body,char *id);
}SERVICE_METHOD_DEF;



extern int  thing_service_parse(cJSON* params,char *method,char *id);
extern void thing_service_reply_ndj_lane_set(uint8_t result,char *id);
extern void thing_service_reply_ndj_lane_get(uint8_t lane_id,uint8_t status,uint8_t coins,uint8_t time,char *id);

extern void thing_service_reply_pay_apply_ndj(uint8_t id,char *order,uint16_t result,char *pack_id);
extern void thing_service_reply_pay_start_ndj(uint8_t id,char *order,uint16_t result,char *pack_id);


extern void thing_service_reply_pay_apply(char *order,uint16_t result,char *pack_id);


extern void thing_service_reply_pay_wwj(char *order,uint16_t result,char *pack_id);
extern void thing_service_reply_pay_apply_dbj(char *order,uint16_t result,char *pack_id);
extern void thing_service_reply_pay_start_dbj(char *order,uint16_t result,char *pack_id,uint16_t coins,double money);

extern void thing_service_reply_pay_start_amy(char *order,uint16_t result,char *pack_id);
extern void thing_service_reply_pay_start_shj(char *order, char *itemOrder, char *positionId, uint16_t positionX, uint16_t positionY, uint16_t result, uint16_t number, uint16_t stock, char *pack_id);

#endif // ! 

