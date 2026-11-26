/** @file lyy_pulses.h
 *  @brief 双货道脉冲设备的货道、套餐缓存及云端同步接口。
 */
#ifndef LYY_PULSES_H
#define LYY_PULSES_H

#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"
#include "lyy_para.h"

/** @brief 双货道设备支持的物理货道数。 */
#define PULSES_LANE_COUNT 2
/** @brief 每个货道最多可缓存的有效套餐数。 */
#define PULSES_PACKAGE_MAX 6

/**
 * @brief 复制指定货道的名称。
 * @param position 物理货道编号 1 或 2。
 * @param[out] name 名称缓冲区。
 * @param capacity 缓冲区的字节数，包含结束符。
 * @return 货道已配置为 1；未就绪或参数无效为 0。
 */
int pulses_get_lane_name(uint8_t position, char *name, size_t capacity);
/**
 * @brief 查询可展示的套餐数量。
 * @param position 物理货道编号 1 或 2。
 * @return 有效套餐数；关闭、未就绪、无套餐或编号无效时为 0。
 */
uint8_t pulses_get_package_count(uint8_t position);
/**
 * @brief 复制指定货道和槽位的套餐及可选币种文本。
 * @param position 物理货道编号 1 或 2。
 * @param slot 从 0 开始的套餐槽位。
 * @param[out] out 套餐快照。
 * @param[out] currency 币种缓冲区，可为 NULL。
 * @param currency_capacity 币种缓冲区的字节数，包含结束符。
 * @return 复制成功为 1；货道关闭、未就绪或槽位无效时为 0。
 */
int pulses_get_package(uint8_t position, uint8_t slot, PAY_PACKAGE_DEF *out,
                       char *currency, size_t currency_capacity);
/** @brief 返回缓存修订号，用于屏幕检测数据变更。 */
uint32_t pulses_get_revision(void);
/**
 * @brief 校验可按原样写成 JSON 数字的套餐 ID。
 * @param id 以 NUL 结尾的十进制 ID 文本。
 * @return 合法为 1，否则为 0。
 */
int pulses_valid_package_id(const char *id);

/**
 * @brief 推进云端同步，先查货道，收到有效货道信息后再查套餐。
 * @param connected MQTT 已连接为非 0，断开为 0。
 * @note 断线后旧数据不再向屏幕提供，重连后重新同步。
 */
void pulses_sync_poll(int connected);
/**
 * @brief 合并货道名称和布尔型 enable，保留未列出的货道及未下发的字段。
 * @param params position_info 属性数组；各项是 JSON 字符串。
 * @return 有效货道更新为 1，未应用为 -1。
 * @note 关闭货道不删除已缓存的套餐，重新开启可直接恢复显示。
 */
int pulses_write_positions(const cJSON *params);
/**
 * @brief 处理联网查询返回的套餐属性，沿用同步路径的空列表语义。
 * @param params position_pay_package 属性值，支持对象或 JSON 字符串。
 * @return 已应用为 1，未应用为 -1。
 */
int pulses_write_packages(const cJSON *params);
/**
 * @brief 合并后台主动推送的货道套餐。
 * @param params position_pay_package 属性值，支持对象或 JSON 字符串。
 * @details 未下发或 consumptionPackage 为空的货道保留旧套餐。
 * @return 已更新或有效空更新为 1，无效或未应用为 -1。
 */
int pulses_update_packages(const cJSON *params);

#endif
