#include "lyy_frame.h"
#include <string.h>

static void discard(lyy_frame_stream *s, size_t count)
{
    s->used -= count;
    memmove(s->data, s->data + count, s->used);
}

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
