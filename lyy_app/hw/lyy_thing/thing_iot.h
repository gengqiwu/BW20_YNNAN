/*
 * @author: e.he
 * @Date: 2024-06-14 10:54:17
 * @LastEditors: linjunqi linjunqi@leyaoyao.org
 * @LastEditTime: 2024-07-31 20:49:29
 * @Description: 
 */
#ifndef __THING_IOT_H__
#define __THING_IOT_H__

#include "cJSON.h"

extern void lyy_thing_down_parse(char *payload, uint32_t payload_len, char *topic);

//属性
extern void thing_property_post(cJSON* params);
extern void property_read_reply(cJSON* params, char *message);
extern void thing_property_set_reply(char *msg,char *id);
//服务
extern void thing_service_invoke_reply(cJSON* data, char *message, char *id, char *identifier);

//事件
extern void thing_event_post(cJSON* value, char *event);


//ota
extern void thing_ota_inform_version(char *version, char* module);
//主板参数
extern void lyy_thing_ctlb_up_raw(uint8_t *data, uint16_t len);
extern void ctlb_up_raw_board_error(uint8_t err); //故障上报

//设备属性
extern void thing_device_infor_query(void);
extern void lyy_thing_raw_up(uint8_t *data, uint16_t len);

extern char *get_random_string(void);


#endif


