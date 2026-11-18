#ifndef LYY_FRAME_H
#define LYY_FRAME_H
#include <stdint.h>
#include <stddef.h>
#define LYY_FRAME_CAPACITY 1280U

typedef void (*lyy_frame_callback)(uint8_t *frame, uint16_t length);
typedef struct {
    uint8_t data[LYY_FRAME_CAPACITY];
    size_t used;
    uint32_t last_tick;
} lyy_frame_stream;

/* Accept arbitrary UART chunks; discard stale partial frames after 200 ms. */
void lyy_frame_feed(lyy_frame_stream *stream, const uint8_t *data, size_t length,
                    uint32_t tick, lyy_frame_callback callback);
#endif
