/** @file lyy_frame.c @brief 串口半包累积、粘包拆分和校验错误后的帧头重同步。 */
#include "lyy_frame.h"
#include <string.h>

/**
 * @brief 丢弃缓存头部指定数量的字节并前移剩余数据。
 * @param[in,out] s 接收流。
 * @param[in] count 丢弃字节数，调用者保证不大于 s->used。
 */
static void discard(lyy_frame_stream *s, size_t count)
{
    s->used -= count;
    memmove(s->data, s->data + count, s->used);
}

/**
 * @brief 消费缓存中的完整合法帧，遇到未接收完整的帧时保留并返回。
 * @param[in,out] s 当前接收流，回调返回后会移动内部缓存。
 * @param[in] callback 可为空的同步业务回调。
 * @note D6 屏幕长度位于字节 5/6，CD 长度位于字节 4/5，均为小端，不能混用。
 */
static void drain(lyy_frame_stream *s, lyy_frame_callback callback)
{
    while (s->used) {
        uint8_t *p = s->data;
        if (p[0] != 0xAA) { discard(s, 1); continue; }
        if (s->used < 2) return;
        size_t total;
        if (p[1] != 0xFF) {
            if (p[1] < 3) { discard(s, 1); continue; }
            total = (size_t)p[1] + 3;
        } else {
            if (s->used < 7) return;
            if (p[3] == 0xD6 && p[4] == 0x01)
                total = ((size_t)p[5] | ((size_t)p[6] << 8)) + 9;
            else if (p[3] == 0xCD)
                total = ((size_t)p[4] | ((size_t)p[5] << 8)) + 8;
            else { discard(s, 1); continue; }
        }
        if (total < 6 || total > sizeof(s->data)) { discard(s, 1); continue; }
        if (s->used < total) return;
        uint8_t checksum = 0;
        for (size_t i = 1; i < total - 2; ++i) checksum ^= p[i];
        if (p[total - 1] != 0xDD || checksum != p[total - 2]) {
            discard(s, 1);
            continue;
        }
        if (callback) callback(p, (uint16_t)total);
        discard(s, total);
    }
}

/** @brief 流式接收入口，参数与缓存生命周期约定见 lyy_frame.h。 */
void lyy_frame_feed(lyy_frame_stream *s, const uint8_t *data, size_t len,
                    uint32_t tick, lyy_frame_callback callback)
{
    if (!s || (!data && len)) return;
    if ((uint32_t)(tick - s->last_tick) >= 200) s->used = 0;
    s->last_tick = tick;
    while (len) {
        if (s->used == sizeof(s->data)) discard(s, 1);
        size_t n = sizeof(s->data) - s->used;
        if (n > len) n = len;
        memcpy(s->data + s->used, data, n);
        s->used += n;
        data += n;
        len -= n;
        drain(s, callback);
    }
}
