/**
 * @file lyy_frame.h
 * @brief 与 RTOS 无关的 STM32 串口流式分帧接口。
 */
#ifndef LYY_FRAME_H
#define LYY_FRAME_H
#include <stdint.h>
#include <stddef.h>
/** @brief 单个接收流的最大缓存字节数，超长帧不会递交业务解析。 */
#define LYY_FRAME_CAPACITY 1280U

/**
 * @brief 有效完整帧回调，包含帧头、校验和帧尾。
 * @param[in,out] frame 指向接收流内部缓存，仅在本次回调执行期间有效。
 * @param[in] length 完整帧字节数。
 * @warning 如需异步处理必须复制数据；不得递归向同一流投递数据。
 */
typedef void (*lyy_frame_callback)(uint8_t *frame, uint16_t length);
/** @brief 单串口接收流状态，首次使用前整体清零，所有访问由调用方串行化。 */
typedef struct {
    uint8_t data[LYY_FRAME_CAPACITY]; /**< 尚未消费的串口数据。 */
    size_t used;                    /**< data 中有效字节数。 */
    uint32_t last_tick;              /**< 上次输入时的系统毫秒计数，可自然回绕。 */
} lyy_frame_stream;

/**
 * @brief 累积任意 UART 接收块，校验并同步递交其中的完整帧。
 * @param[in,out] stream 已清零初始化的接收流状态。
 * @param[in] data 本次输入数据，length 为 0 时可为 NULL。
 * @param[in] length 输入字节数，可包含多帧或半帧。
 * @param[in] tick 当前毫秒计数；相邻输入间隔达到 200 ms 时丢弃旧缓存。
 * @param[in] callback 完整帧回调；NULL 时仍消费有效帧，但不递交。
 * @note 支持普通 AA/LEN 帧、D6/01 屏幕扩展帧及 CD 升级扩展帧。
 *       错误帧逐字节重同步，XOR 覆盖 LEN 至最后一个载荷字节。
 */
void lyy_frame_feed(lyy_frame_stream *stream, const uint8_t *data, size_t length,
                    uint32_t tick, lyy_frame_callback callback);
#endif
