#include <float.h>
#include <stdio.h>
#include <string.h>
#include "HAL_System.h"
#include "lyy_pulses.h"
#include "thing_event.h"

#define PULSES_SYNC_RETRY_MS 5000U
#define PULSES_NAME_SIZE 64
#define PULSES_CURRENCY_SIZE 64

typedef struct {
    uint8_t count[PULSES_LANE_COUNT];
    char currency[PULSES_CURRENCY_SIZE];
    char currency_symbol[PULSES_CURRENCY_SIZE];
    PAY_PACKAGE_DEF packages[PULSES_LANE_COUNT][PULSES_PACKAGE_MAX];
} PULSES_PACKAGES;

/* Writers run in MQTT tasks and readers in the screen task. Only publish or
 * copy snapshots under the lock; parse and send MQTT outside the lock. */
static struct {
    uint32_t revision;
    uint32_t session;
    uint32_t last_query;
    uint8_t connected;
    uint8_t positions;
    uint8_t query_stage;
    uint8_t packages_ready;
    char names[PULSES_LANE_COUNT][PULSES_NAME_SIZE];
    PULSES_PACKAGES data;
} s_pulses;

static int valid_position(uint8_t position)
{
    return position >= 1 && position <= PULSES_LANE_COUNT;
}

static int position_ready(uint8_t position)
{
    return valid_position(position) && (s_pulses.positions & (1U << (position - 1)));
}

static void copy_text(char *dest, size_t capacity, const char *text)
{
    if (!capacity) return;
    size_t n = strlen(text);
    if (n >= capacity) {
        n = capacity - 1;
        while (n && ((unsigned char)text[n] & 0xc0) == 0x80) --n;
    }
    memcpy(dest, text, n);
    dest[n] = '\0';
}

int pulses_get_lane_name(uint8_t position, char *name, size_t capacity)
{
    if (!name || !capacity) return 0;
    taskENTER_CRITICAL();
    int ready = position_ready(position);
    copy_text(name, capacity, ready ? s_pulses.names[position - 1] : "");
    taskEXIT_CRITICAL();
    return ready;
}

uint8_t pulses_get_package_count(uint8_t position)
{
    taskENTER_CRITICAL();
    uint8_t count = s_pulses.packages_ready && position_ready(position)
                  ? s_pulses.data.count[position - 1] : 0;
    taskEXIT_CRITICAL();
    return count;
}

int pulses_get_package(uint8_t position, uint8_t slot, PAY_PACKAGE_DEF *out,
                       char *currency, size_t currency_capacity)
{
    if (!out) return 0;
    taskENTER_CRITICAL();
    int ready = s_pulses.packages_ready && position_ready(position) &&
                slot < s_pulses.data.count[position - 1];
    if (ready) {
        *out = s_pulses.data.packages[position - 1][slot];
        if (currency) {
            copy_text(currency, currency_capacity, *s_pulses.data.currency
                      ? s_pulses.data.currency : s_pulses.data.currency_symbol);
        }
    }
    taskEXIT_CRITICAL();
    return ready;
}

uint32_t pulses_get_revision(void)
{
    taskENTER_CRITICAL();
    uint32_t revision = s_pulses.revision;
    taskEXIT_CRITICAL();
    return revision;
}

void pulses_sync_poll(int connected)
{
    uint8_t query = 0;
    uint32_t now = HAL_ulGetSysTick();
    taskENTER_CRITICAL();
    if (!connected) {
        if (s_pulses.connected) {
            s_pulses.connected = 0;
            s_pulses.positions = 0;
            s_pulses.query_stage = 0;
            s_pulses.packages_ready = 0;
            ++s_pulses.session;
            ++s_pulses.revision;
        }
    } else {
        s_pulses.connected = 1;
        if (!s_pulses.packages_ready) {
            uint8_t stage = s_pulses.positions ? 2U : 1U;
            if (s_pulses.query_stage != stage || now - s_pulses.last_query >= PULSES_SYNC_RETRY_MS) {
                query = s_pulses.query_stage = stage;
                s_pulses.last_query = now;
            }
        }
    }
    taskEXIT_CRITICAL();
    if (query == 1) event_post_query_lane_info_post();
    if (query == 2) event_post_query_pay_packages_ndj("CONSUMPTION", 1, 0, "true");
}

static int read_coordinate(const cJSON *item, int expected)
{
    return (cJSON_IsNumber(item) && item->valuedouble == expected) ||
           (cJSON_IsString(item) && item->valuestring &&
            item->valuestring[0] == '0' + expected && item->valuestring[1] == '\0');
}

static uint8_t read_position(const cJSON *object)
{
    if (!read_coordinate(cJSON_GetObjectItemCaseSensitive(object, "positionY"), 0)) return 0;
    const cJSON *x = cJSON_GetObjectItemCaseSensitive(object, "positionX");
    for (uint8_t i = 1; i <= PULSES_LANE_COUNT; ++i) {
        if (read_coordinate(x, i)) return i;
    }
    return 0;
}

int pulses_valid_package_id(const char *id)
{
    if (!id || *id < '0' || *id > '9' || (*id == '0' && id[1])) return 0;
    for (size_t i = 0; id[i]; ++i) {
        if (i >= sizeof(((PAY_PACKAGE_DEF *)0)->id) - 1 || id[i] < '0' || id[i] > '9') return 0;
    }
    return 1;
}

static const char *text_field(const cJSON *body, const char *key)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(body, key);
    return cJSON_IsString(value) && value->valuestring ? value->valuestring : "";
}

int pulses_write_positions(const cJSON *params)
{
    if (!cJSON_IsArray(params)) return -1;
    taskENTER_CRITICAL();
    uint32_t session = s_pulses.session;
    int connected = s_pulses.connected;
    taskEXIT_CRITICAL();
    if (!connected) return -1;

    char names[PULSES_LANE_COUNT][PULSES_NAME_SIZE] = {{0}};
    uint8_t mask = 0;
    const cJSON *entry;
    cJSON_ArrayForEach(entry, params) {
        if (!cJSON_IsString(entry) || !entry->valuestring) continue;
        cJSON *body = cJSON_ParseWithOpts(entry->valuestring, NULL, 1);
        uint8_t position = read_position(body);
        const char *name = text_field(body, "positionName");
        if (valid_position(position) && *name) {
            copy_text(names[position - 1], sizeof(names[0]), name);
            mask |= 1U << (position - 1);
        }
        cJSON_Delete(body);
    }
    if (!mask) return -1;

    int result = -1;
    taskENTER_CRITICAL();
    if (s_pulses.connected && session == s_pulses.session) {
        for (uint8_t i = 0; i < PULSES_LANE_COUNT; ++i) {
            if (mask & (1U << i)) memcpy(s_pulses.names[i], names[i], sizeof(names[i]));
        }
        s_pulses.positions |= mask;
        ++s_pulses.revision;
        result = 1;
    }
    taskEXIT_CRITICAL();
    return result;
}

static int read_amount(const cJSON *entry, const char *key, double *out)
{
    const cJSON *value = cJSON_GetObjectItemCaseSensitive(entry, key);
    if (!cJSON_IsNumber(value) || !(value->valuedouble >= 0 && value->valuedouble <= FLT_MAX)) return 0;
    *out = value->valuedouble;
    return 1;
}

static int read_package(const cJSON *entry, PAY_PACKAGE_DEF *pkg)
{
    const char *id = text_field(entry, "id");
    double price, value;
    if (!pulses_valid_package_id(id) || !read_amount(entry, "price", &price) ||
        !read_amount(entry, "priceValue", &value)) return 0;
    copy_text(pkg->id, sizeof(pkg->id), id);
    copy_text(pkg->name, sizeof(pkg->name), text_field(entry, "name"));
    copy_text(pkg->unit, sizeof(pkg->unit), text_field(entry, "priceValueUnitDesc"));
    copy_text(pkg->commodity_Desc, sizeof(pkg->commodity_Desc), text_field(entry, "commodityDesc"));
    copy_text(pkg->price_Desc, sizeof(pkg->price_Desc), text_field(entry, "priceDesc"));
    const cJSON *seq = cJSON_GetObjectItemCaseSensitive(entry, "seq");
    if (cJSON_IsNumber(seq) && seq->valueint > 0) pkg->seq = (uint32_t)seq->valueint;
    pkg->price = (float)price;
    pkg->priceValue = (float)value;
    const char *display = text_field(entry, "displayPrice");
    if (*display) copy_text(pkg->display_price, sizeof(pkg->display_price), display);
    else snprintf(pkg->display_price, sizeof(pkg->display_price), "%.12g", price);
    display = text_field(entry, "displayPriceValue");
    if (*display) copy_text(pkg->display_value, sizeof(pkg->display_value), display);
    else snprintf(pkg->display_value, sizeof(pkg->display_value), "%.12g", value);
    return 1;
}

int pulses_write_packages(const cJSON *params)
{
    taskENTER_CRITICAL();
    uint32_t session = s_pulses.session;
    int requested = s_pulses.connected && s_pulses.positions && s_pulses.query_stage == 2;
    taskEXIT_CRITICAL();
    if (!requested) return -1;

    cJSON *parsed = NULL;
    if (cJSON_IsString(params)) {
        parsed = cJSON_ParseWithOpts(params->valuestring, NULL, 1);
        params = parsed;
    }
    const cJSON *list = cJSON_GetObjectItemCaseSensitive(params, "positionList");
    if (!cJSON_IsArray(list)) {
        cJSON_Delete(parsed);
        return -1;
    }

    PULSES_PACKAGES *next = cJSON_malloc(sizeof(*next));
    if (!next) {
        cJSON_Delete(parsed);
        return -1;
    }
    memset(next, 0, sizeof(*next));
    copy_text(next->currency, sizeof(next->currency), text_field(params, "currencyCode"));
    copy_text(next->currency_symbol, sizeof(next->currency_symbol), text_field(params, "currencySymbol"));
    const cJSON *lane, *entry;
    uint8_t seen = 0;
    cJSON_ArrayForEach(lane, list) {
        uint8_t position = read_position(lane);
        if (!valid_position(position)) continue;
        uint8_t index = position - 1;
        uint8_t mask = 1U << index;
        const cJSON *packages = cJSON_GetObjectItemCaseSensitive(lane, "consumptionPackage");
        if ((seen & mask) || !cJSON_IsArray(packages)) continue;
        seen |= mask;
        uint8_t *count = &next->count[index];
        cJSON_ArrayForEach(entry, packages) {
            if (*count == PULSES_PACKAGE_MAX) break;
            if (read_package(entry, &next->packages[index][*count])) ++*count;
        }
    }
    int result = -1;
    taskENTER_CRITICAL();
    if ((seen || !list->child) && s_pulses.connected && s_pulses.positions && session == s_pulses.session) {
        /* A single-lane update must not wipe the other lane. An explicit
         * empty list is a full clear; a new session starts empty as well. */
        if (!s_pulses.packages_ready || !list->child) {
            s_pulses.data = *next;
        } else {
            for (uint8_t i = 0; i < PULSES_LANE_COUNT; ++i) {
                if (!(seen & (1U << i))) continue;
                s_pulses.data.count[i] = next->count[i];
                memcpy(s_pulses.data.packages[i], next->packages[i], sizeof(next->packages[i]));
            }
            if (*next->currency) memcpy(s_pulses.data.currency, next->currency, sizeof(next->currency));
            if (*next->currency_symbol) memcpy(s_pulses.data.currency_symbol, next->currency_symbol, sizeof(next->currency_symbol));
        }
        s_pulses.packages_ready = 1;
        ++s_pulses.revision;
        result = 1;
    }
    taskEXIT_CRITICAL();
    cJSON_free(next);
    cJSON_Delete(parsed);
    return result;
}
