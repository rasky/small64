#include <stdint.h>
#include <stdbool.h>
#include "minidragon.h"
#include "minilib.h"
#include "stage1.h"
#include "ay8910.h"
#include "rdp_commands.h"
#include "rdpq_macros.h"
#include <math.h>

// 0:PAL, 1:NTSC, 2:MPAL
#ifndef VIDEO_TYPE
#error VIDEO_TYPE not defined
#endif

#ifdef DEBUG
#define DEBUG_NOINLINE   __attribute__((noinline))
#else
#define DEBUG_NOINLINE
#endif

#define UNCACHED    __attribute__((__section__(".uncached"), __aligned__(16)))

#define SWAP(a, b)  ({ typeof(a) _tmp = a; a = b; b = _tmp; })
#define CLAMP(x, min, max)  ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

#define FB_BUFFER_0         ((void*)0xA0100000)  // len = 320*240*2, end = 0xA0125800
#define FB_BUFFER_1         ((void*)0xA0130000)  // len = 320*240*2, end = 0xA014B000
#define FB_BUFFER_2         ((void*)0xA0160000)

#define AI_BUFFERS          ((void*)0xA0190800)
#define DELAY_BUFFER        ((void*)0x801A0000)

#define TEXTURE_BUFFER      ((void*)0xA0200000)

#define VERTEX_BUFFER       ((void*)0xA0210000)
#define RDP_BUFFER          ((void*)0xA0220000)
#define Z_BUFFER            ((void*)0xA03D0000)

#define AI_BUFFER_SIZE              12800
#include "music.c"
#define AI_FREQUENCY                SONG_FREQUENCY

#define FB_WIDTH      320
#define FB_HEIGHT     240
#define FB_SCALE_X    (FB_WIDTH * 1024 / 640)
#define FB_SCALE_Y    (FB_HEIGHT * 1024 / 240)

#define RGBA16(r,g,b,a)   (((r)<<11) | ((g)<<6) | ((b)<<1) | (a))
#define RGBA32(r,g,b,a)   (((int)(r)<<24) | ((int)(g)<<16) | ((int)(b)<<8) | (int)(a))

#ifdef DEBUG
extern int get_tv_type(void);
extern bool sys_bbplayer(void);
#else
static int get_tv_type(void) { return *IPL3_TV_TYPE; }
static bool sys_bbplayer(void) { return (*MI_VERSION & 0xF0) == 0xB0; }
#endif

void *vi_buffer_draw;
void *vi_buffer_show;
int vi_buffer_draw_idx;
int framecount;
uint32_t vblank_time;

#if VIDEO_TYPE == 0    // PAL
#define TIME_30FPS     (93750000 / 2 / 25)
#else // NTSC,MPAL
#define TIME_30FPS     (93750000 / 2 / 30)
#endif

static const uint32_t vi_regs_p[14] = {
#if VIDEO_TYPE == 0
    /* PAL */
    0x3202, (uint32_t)FB_BUFFER_0,
    FB_WIDTH, 0, 0,
    0x0404233a, 0x00000271, 0x00150c69,
    0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b,
    FB_SCALE_X, FB_SCALE_Y,
#elif VIDEO_TYPE == 1
    /* NTSC */
    0x3202, (uint32_t)FB_BUFFER_0,
    FB_WIDTH, 0, 0,
    0x03e52239, 0x0000020d, 0x00000c15,
    0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204,
    FB_SCALE_X, FB_SCALE_Y,
#elif VIDEO_TYPE == 2
    /* MPAL */
    0x3202, (uint32_t)FB_BUFFER_0,
    FB_WIDTH, 0, 0,
    0x04651e39, 0x0000020d, 0x00040c11,
    0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204,
    FB_SCALE_X, FB_SCALE_Y,
#else
#error Unsupported VIDEO_TYPE
#endif
};

static void vi_reset(int regstart)
{
    volatile uint32_t* VI_REGS = (uint32_t*)0xA4400000;

    #pragma GCC unroll 0
    for (int reg=regstart; reg<14; reg++)
        VI_REGS[reg] = vi_regs_p[reg];
}

static void vi_init(void)
{
    vi_reset(0);
    *VI_ORIGIN = (uint32_t)FB_BUFFER_0;
    while (*VI_V_CURRENT != 2) {}
    vblank_time = C0_COUNT();
    vi_buffer_draw = FB_BUFFER_0;
    vi_buffer_show = FB_BUFFER_1;
}

static void vi_wait_vblank(void)
{
    // wait for line change at the beginning of the vblank
    uint32_t target = vblank_time + TIME_30FPS;
    while (C0_COUNT() < target) {}
    while (*VI_V_CURRENT != 2) {}
    vblank_time = C0_COUNT();
    framecount++;

    *VI_ORIGIN = (uint32_t)vi_buffer_draw;
    SWAP(vi_buffer_draw, vi_buffer_show);
}

#define AI_CALC_DACRATE(clock)      (((2 * (clock) / AI_FREQUENCY) + 1) / 2)
#define AI_CALC_BITRATE(clock)      ((AI_CALC_DACRATE(clock) / 66) > 16 ? 16 : (AI_CALC_DACRATE(clock) / 66))

static uint32_t ai_buffer_offset;

static int16_t* ai_poll(void)
{
    if (*MI_INTERRUPT & MI_INTERRUPT_AI) {
        *AI_STATUS = AI_CLEAR_INTERRUPT;
        return (int16_t*)ai_buffer_offset;
    }
    return 0;
}

static void ai_poll_end(void)
{
    *AI_DRAM_ADDR = ai_buffer_offset;
    *AI_LENGTH = AI_BUFFER_SIZE;
    *AI_CONTROL = 1;
    ai_buffer_offset ^= AI_BUFFER_SIZE;
}

static void ai_init(void)
{
#if VIDEO_TYPE == 0
    *AI_DACRATE = AI_CALC_DACRATE(49656530)-1;
#elif VIDEO_TYPE == 1
    *AI_DACRATE = AI_CALC_DACRATE(48681818)-1;
#elif VIDEO_TYPE == 2
    *AI_DACRATE = AI_CALC_DACRATE(48628322)-1;
#endif
    *AI_BITRATE = 0xf;  // up to 44100; then you might need 0xe. Use AI_CALC_BITRATE to calculate the correct value
    ai_buffer_offset = (uint32_t)AI_BUFFERS;
    ai_poll_end(); // force a first empty buffer to be played back
}

static void dp_wait(void)
{
    while (*DP_STATUS & DP_STATUS_PIPE_BUSY) {};
}

__attribute__((noinline))
static void dp_send(void *dl, void *dl_end)
{
    *DP_START = (uint32_t)dl;
    *DP_END = (uint32_t)(dl_end);
    dp_wait();
}

#include "direction.c"

void dp_begin_frame(void)
{
    static RdpList dlist[] = {
        RdpSetColorImage(RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, 320, 0),
        RdpSetZImage(Z_BUFFER),
        RdpSetClippingI(0, 0, 320, 240),
        RdpSetOtherModes(SOM_CYCLE_FILL),
        RdpSetFillColor16(0),
        RdpFillRectangleI(0, 0, 320, 240),
        RdpSetOtherModes(SOM_CYCLE_1 | SOM_ZSOURCE_PRIM | SOM_Z_WRITE | SOM_RGBDITHER_NOISE | SOM_ALPHACOMPARE_NOISE),
        RdpSetCombine(RDPQ_COMBINER_FLAT),
        RdpSetPrimDepth(0xFFFE),
        RdpSetPrimColor(RGBA32(0x30, 0x30, 0x30, 0x20)),
 [10] = RdpFillRectangleI(0, 0, 320, 240),
        RdpSyncFull(),
    };

    uint32_t *udl = (uint32_t*)((uint32_t)dlist | 0xA0000000);
    udl[1] = (uint32_t)vi_buffer_draw;
    // blank the second fillrect; this is just to save precious fill time
    // can be disabled for codesize
    //if (framecount > T_FRACTAL) udl[10*2] = 0;
    dp_send(dlist, dlist + sizeof(dlist)/sizeof(uint64_t));
}


void music_poll(void)
{
    int16_t *ai_buffer = ai_poll();
    if (ai_buffer) {
        music_render(ai_buffer);
        ai_poll_end();
    }
}

//#include "noise.c"
#include "ucode.c"
#include "scroller.c"
//#include "bkg.c"
#include "mesh.c"
#include "fractal.c"

extern void gentorus(void* buffer);

__attribute__((used))
void demo(void)
{
    ai_init();
    vi_init();
    ucode_init();
    music_init();
    gentorus(VERTEX_BUFFER);

    //skip to a certain scene:
    framecount = T_MESH2;
    currentRow = framecount * AI_FREQUENCY / (AI_BUFFER_SIZE/4) / 25;
    int intro_phidx = 0;
    while(1) {
        vi_wait_vblank();
        if (framecount > T_END) {
            *VI_H_VIDEO = 0x0;
            while(1){}
        }

        intro_phidx = draw_intro_setup();
        debugf("framecount: %d\n", framecount*60/50*2);

        dp_begin_frame();

        if (intro_phidx >= 0) {
            draw_intro(intro_phidx);
        }
        if (framecount > T_NOISE2) {
            music_poll();
            continue;
        }

        if (framecount > T_FRACTAL) {
            fracgen_draw();
        }

        if (framecount > T_MESH) {
            mesh_setup();
            mesh_draw_async(framecount > T_MESH2 ? 2 : 1);
        }

        music_poll();

        if (framecount > T_MESH) {
           mesh_draw_wait();
        }

        // draw_scroller(vi_buffer_draw);
        if (framecount > T_CREDITS) {
            draw_credits();
        }
    }
    while(1) {}
}
