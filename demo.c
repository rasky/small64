#include <stdint.h>
#include <stdbool.h>
#include "minidragon.h"
#include "minilib.h"
#include "stage1.h"
#include "ay8910.h"
#include <math.h>

#define SWAP(a, b)  ({ typeof(a) _tmp = a; a = b; b = _tmp; })
#define CLAMP(x, min, max)  ((x) < (min) ? (min) : (x) > (max) ? (max) : (x))

#include "bbsong1.c"
#define AI_FREQUENCY                SONG_FREQUENCY

#define FB_BUFFER_0         ((void*)0xA0100000)  // len = 320*240*2, end = 0xA0125800
#define FB_BUFFER_1         ((void*)0xA0125800)  // len = 320*240*2, end = 0xA014B000
#define FB_BUFFER_2         ((void*)0xA014B000)  // len = 320*240*2, end = 0xA0170800

#define AI_BUFFERS          ((void*)0xA0170800)
#define AI_BUFFER_SIZE      4096

#define TEXTURE_BUFFER      ((void*)0xA0200000)

#define VERTEX_BUFFER       ((void*)0xA0210000)
#define RDP_BUFFER          ((void*)0xA0220000)
#define Z_BUFFER            ((void*)0xA03D0000)
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

static void vi_init(void)
{
    static const uint32_t vi_regs_p[3][7] =  {
    {   /* PAL */   0x0404233a, 0x00000271, 0x00150c69,
        0x0c6f0c6e, 0x00800300, 0x005f0239, 0x0009026b },
    {   /* NTSC */  0x03e52239, 0x0000020d, 0x00000c15,
        0x0c150c15, 0x006c02ec, 0x002501ff, 0x000e0204 },
    {   /* MPAL */  0x04651e39, 0x0000020d, 0x00040c11,
        0x0c190c1a, 0x006c02ec, 0x002501ff, 0x000e0204 },
    };

    volatile uint32_t* regs = (uint32_t*)0xA4400000;
    regs[1] = (uint32_t)FB_BUFFER_0;
    regs[2] = 320;
    regs[12] = 0x200;
    regs[13] = 0x400;

    int tv_type = get_tv_type();
    #pragma GCC unroll 0
    for (int reg=0; reg<7; reg++)
        regs[reg+5] = vi_regs_p[tv_type][reg];
    regs[0] = sys_bbplayer() ? 0x1202 : 0x3202;

    vi_buffer_draw = FB_BUFFER_0;
    vi_buffer_show = FB_BUFFER_1;
}

static void vi_wait_vblank(void)
{
    while (*VI_V_CURRENT != 2) {}
    *VI_ORIGIN = (uint32_t)vi_buffer_draw;
    SWAP(vi_buffer_draw, vi_buffer_show);
}

#define AI_CALC_DACRATE(clock)      (((2 * (clock) / AI_FREQUENCY) + 1) / 2)
#define AI_CALC_BITRATE(clock)      ((AI_CALC_DACRATE(clock) / 66) > 16 ? 16 : (AI_CALC_DACRATE(clock) / 66))

static int ai_buffer_offset;

static int16_t* ai_poll(void)
{
    if (*MI_INTERRUPT & MI_INTERRUPT_AI) {
        *AI_STATUS = AI_CLEAR_INTERRUPT;
        return AI_BUFFERS + ai_buffer_offset;
    }
    return 0;
}

static void ai_poll_end(void)
{
    *AI_DRAM_ADDR = (uint32_t)AI_BUFFERS + ai_buffer_offset;
    *AI_LENGTH = AI_BUFFER_SIZE;
    ai_buffer_offset ^= AI_BUFFER_SIZE;
}

static void ai_init(void)
{
    static const uint16_t bitrate[3] = { AI_CALC_DACRATE(49656530)-1, AI_CALC_DACRATE(48681818)-1, AI_CALC_DACRATE(48628322)-1 };
    *AI_DACRATE = bitrate[get_tv_type()];
    *AI_BITRATE = 0xf;  // up to 44100; then you might need 0xe. Use AI_CALC_BITRATE to calculate the correct value
    *AI_CONTROL = 1;
    ai_buffer_offset = 0;
    memset32(AI_BUFFERS, 0, AI_BUFFER_SIZE * 2);
    ai_poll_end();
}

static void dp_send(void *dl, void *dl_end)
{
    *DP_START = (uint32_t)dl;
    *DP_END = (uint32_t)(dl_end);
}

static void dp_wait(void)
{
    while (*DP_STATUS & DP_STATUS_PIPE_BUSY) {};
}


void bb_render(int16_t *buffer)
{
    static int t = 0;

    for (int i=0; i<AI_BUFFER_SIZE/4; i++) {
        float v = bbgen(t);
        t++;
        buffer[i*2+0] = buffer[i*2+1] = (int16_t)(v * 128.0f);
    }
}

//#include "noise.c"
#include "scroller.c"
#include "bkg.c"
#include "mesh.c"
#include "ucode.c"

__attribute__((used))
void demo(void)
{
    ai_init();
    vi_init();
    //ym_init();
    ucode_init();

#if 0
    init_perlin();
    
    #define TW   32
    for (int y=0;y<TW;y++) {
        for (int x=0;x<TW;x++) {
            float noise = perlin((float)x*8 / TW, (float)y / TW);
            int value = (int)((noise + 1.0f) * 0.5f * 32);

            uint16_t pixel = RGBA(value, value, value, 1);
            ((uint16_t*)FB_BUFFER_0)[y * 320 + x] = pixel;
        }
    }
#endif

    float dispTimer = -3;
    
    while(1) {
        vi_wait_vblank();
        memset32(Z_BUFFER, (ZBUF_MAX<<16)|ZBUF_MAX, 320*240*2);
        draw_bkg();

        //uint32_t t = C0_COUNT();

        xangle += 0.01f;
        yangle += 0.015f;
        uint32_t vert_buff_end = mesh();
        ucode_set_srt(1.0f, (float[]){xangle, yangle, 0.0f}, 160<<2, 120<<2);

        *DP_STATUS = DP_WSTATUS_SET_XBUS;
        *DP_START = 0x30; // @TODO: why do i have to set both here? (hangs otherwise)
        *DP_END = 0x30;

        float dispFactor = mm_sinf(__builtin_fmaxf(dispTimer, 0));
        ucode_set_displace(dispFactor * 0x7FFF); 
        if(dispTimer > MM_PI*2)dispTimer = -2;
        dispTimer += 0.01f;

        ucode_set_vertices_address((uint32_t)VERTEX_BUFFER, vert_buff_end);
        ucode_run();
        dp_wait();

        *DP_STATUS = DP_WSTATUS_CLR_XBUS;

        //t = C0_COUNT() - t;
        //debugf("3D: %ldus\n", TICKS_TO_US(t));
    
        //draw_scroller(vi_buffer_draw);
    
        int16_t *ai_buffer = ai_poll();
         if (ai_buffer) {
             bb_render(ai_buffer);
             ai_poll_end();
         }
    }
    while(1) {}
}
