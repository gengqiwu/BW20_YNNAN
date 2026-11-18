/** @file lyy_mcu_ota.h @brief STM32 D6/0E 固件分包传输与匹配应答接口。 */
#ifndef LYY_MCU_OTA_H
#define LYY_MCU_OTA_H
#include <stdint.h>
/**
 * @brief 从已下载的固件缓存启动独立 STM32 升级发送任务。
 * @pre board_ota_param_t 已填入版本和镜像长度，缓存已完整下载并校验长度。
 * @retval 1 成功创建发送任务，不表示传输完成或新固件已运行。
 * @retval 0 已在传输、长度非法、信号量或任务创建失败。
 * @note 任务逐块等待 ACK；超时停止，不盲目重发，重新发起将从偏移 0 开始。
 */
int lyy_mcu_ota_start(void);
/**
 * @brief 接收任务递交升级应答，仅匹配当前块时释放等待信号量。
 * @param[in] frame 已校验的完整 D6/0E 应答，不可为 NULL。
 * @param[in] length 完整帧长，必须恰好覆盖 34 字节应答及 6 字节外层开销。
 * @note 校验名称、总长、偏移、数据长和成功结果，不推进偏移；推进由发送任务执行。
 */
void lyy_mcu_ota_ack(const uint8_t *frame, uint16_t length);
/** @brief 查询分包发送是否仍在运行。 @return 非零表示忙，0 表示空闲，不含 HTTP 下载阶段。 */
int lyy_mcu_ota_busy(void);
#endif
