#ifndef __THING_SERVICE_H__
#define __THING_SERVICE_H__
#include <stdint.h>
#include "cJSON.h"
typedef struct {
    char method[32];
    int (*parse)(cJSON *body, char *id);
} SERVICE_METHOD_DEF;
char *get_apply_start_result_type(uint16_t code);
char *get_start_type(void);
void set_start_type(char *type);
int thing_service_parse(cJSON *params, char *method, char *id);
int thing_service_pay_start_pulse(cJSON *params, char *id);
void thing_service_reply_ndj_lane_get(uint8_t lane_id, uint8_t status, uint8_t coins, uint8_t time, char *id);
void thing_service_reply_pay_apply(char *order, uint16_t result, char *pack_id);
void thing_service_reply_pay_apply_note(char *order, uint16_t result, char *pack_id);
void thing_service_reply_pay_start_note(char *order, uint16_t result, char *pack_id, uint8_t channel);
#endif
