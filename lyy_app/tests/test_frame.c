/**
 * @file test_frame.c
 * @brief 帧接收器主机回归：任意切分、粘包、错校验、扩展长度、噪声、超时及计时回绕。
 * @note 使用真实 lyy_frame 实现，测试回调只记录合法帧数量和长度。
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "lyy_frame.h"
static unsigned frames;
static uint16_t last_len;
static void received(uint8_t *p, uint16_t n)
{
    assert(p[0] == 0xAA && p[n-1] == 0xDD);
    ++frames;
    last_len = n;
}
static void finish(uint8_t *p, size_t n)
{
    p[n-2] = 0;
    for (size_t i=1; i<n-2; ++i) p[n-2] ^= p[i];
    p[n-1] = 0xDD;
}
int main(void)
{
    uint8_t ordinary[] = {0xAA, 5, 1, 0xD6, 9, 1, 0, 0};
    finish(ordinary, sizeof(ordinary));
    for (size_t split=1; split<sizeof(ordinary); ++split) {
        lyy_frame_stream s = {0}; frames=0;
        lyy_frame_feed(&s, ordinary, split, 10, received);
        assert(frames == 0);
        lyy_frame_feed(&s, ordinary+split, sizeof(ordinary)-split, 20, received);
        assert(frames == 1 && s.used == 0);
    }
    lyy_frame_stream s={0}; frames=0;
    uint8_t joined[32]={0x12,0xDD};
    memcpy(joined+2,ordinary,8); memcpy(joined+10,ordinary,8);
    lyy_frame_feed(&s,joined,18,30,received); assert(frames==2);
    joined[8]^=1;
    frames=0; s.used=0;
    lyy_frame_feed(&s,joined,18,40,received); assert(frames==1);
    uint8_t extended[521]={0xAA,0xFF,1,0xD6,1,0,2};
    memset(extended+7,0xAA,512); finish(extended,sizeof(extended));
    frames=0; s.used=0;
    for(size_t i=0;i<sizeof(extended);++i) lyy_frame_feed(&s,extended+i,1,50,received);
    assert(frames==1 && last_len==521);
    uint8_t ota[540]={0xAA,0xFF,1,0xCD,20,2}; /* 532 + 8 */
    finish(ota,sizeof(ota)); frames=0; s.used=0;
    lyy_frame_feed(&s,ota,sizeof(ota),60,received); assert(frames==1 && last_len==540);
    uint8_t invalid[]={0xAA,0xFF,1,0xD6,1,0xFF,0xFF};
    frames=0; s.used=0;
    lyy_frame_feed(&s,invalid,sizeof(invalid),70,received);
    lyy_frame_feed(&s,ordinary,8,71,received); assert(frames==1);
    s.used=0; frames=0;
    lyy_frame_feed(&s,ordinary,4,100,received);
    lyy_frame_feed(&s,ordinary,8,301,received); assert(frames==1);
    s.used=0; frames=0;
    lyy_frame_feed(&s,ordinary,4,0xFFFFFFF0U,received);
    lyy_frame_feed(&s,ordinary+4,4,10,received); assert(frames==1);
    uint8_t noise[4096]; memset(noise,0xCC,sizeof(noise));
    lyy_frame_feed(&s,noise,sizeof(noise),20,received); assert(s.used==0);
    puts("frame tests passed: splits, coalescing, checksum, extended frames, bounds, timeout, wraparound");
    return 0;
}
