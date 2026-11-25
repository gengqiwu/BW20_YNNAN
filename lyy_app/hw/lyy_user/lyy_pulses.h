#ifndef LYY_PULSES_H
#define LYY_PULSES_H

#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "lyy_para.h"

#define PULSES_LANE_COUNT 2
#define PULSES_PACKAGE_MAX 6

/* Physical positions are 1-based; package slots are 0-based. */
int pulses_get_lane_name(uint8_t position, char *name, size_t capacity);
uint8_t pulses_get_package_count(uint8_t position);
int pulses_get_package(uint8_t position, uint8_t slot, PAY_PACKAGE_DEF *out,
                       char *currency, size_t currency_capacity);
uint32_t pulses_get_revision(void);
int pulses_valid_package_id(const char *id);

void pulses_sync_poll(int connected);
int pulses_write_positions(const cJSON *params);
int pulses_write_packages(const cJSON *params);

#endif
