#include <stdint.h>
#include "rdp_commands.h"
#include "rdpq_macros.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

#define BITMAP_WIDTH     80
#define BITMAP_HEIGHT    120
#define CHUNK_HEIGHT     44

static int texgen_framecount = 0;
static uint8_t* tbuf_calc_frame = TEXTURE_BUFFER;

__attribute__((noinline))
static void fracgen_noisy(uint8_t *tex, int idx, int w, int h)
{
    // tex = (void*)(((uint32_t)tex & 0x1FFFFFFF) | 0x80000000);
    int fc = texgen_framecount;
    int itermax = 16; //MIN((framecount >> 6), 16);

    uint8_t c = 0;
    for (int i=0; i<w*h; i++) {
        int val = idx * ((0xCD00<<1) | c);
        c = 0;

        uint8_t px = (val >> 16) & 0xff;
        uint8_t py = (val >> 24) & 0xff;

        for (int j=0;j<itermax;j++) {
            px += fc;
            py -= px;
            c += px < (py >> 1);
            px -= (py >> 1);
        }

        *tex++ = c<<4;
        // uint8_t r = MIN(c+(c>>1), 0x1F);
        // uint8_t g = MIN(c+(c>>2), 0x1F);
        // uint8_t b = MIN(c*2, 0x1F);
        // *tex++ = RGBA16(r,g,b, 0);

        idx++;
        //if ((i % w) == 0) { idx += 320-w; }
    }
}

static void fracgen(uint8_t *tex, int idx, int len)
{
    // tex = (void*)(((uint32_t)tex & 0x1FFFFFFF) | 0x80000000);
    int fc = texgen_framecount;
    int itermax = MIN(((framecount - 850) >> 6), 10);

    uint32_t c = 0;
    uint8_t* tex_end = tex + len;
    while (tex < tex_end) {
        int val = idx * ((0xCD00<<2) | 0);

        // This orignally used bytes, but it's cheaper to just use the
        // upper bits of a 32-bit value.
        // And we don't bother clearing the lower bits.
        uint32_t px = val << 8;
        uint32_t py = val;

        c = 0;

        for (int j=-itermax;j<0;j++) {
            px -= fc << 24;
            py += px;
            c += px < (py >> 1);
            px -= py >> 1;
        }

        //c += random_u32() & 1;

        *tex++ = c << 4;
        // uint8_t r = MIN(c+(c>>1), 0x1F);
        // uint8_t g = MIN(c+(c>>2), 0x1F);
        // uint8_t b = MIN(c*2, 0x1F);
        // *tex++ = RGBA16(r,g,b, 0);

        idx++;
        //if ((i % w) == 0) { idx += 320-w; }
    }
}

#define DRAW_BITMAP_CHUNK(n) \
    RdpLoadTileI(TILE0, 0, MAX(CHUNK_HEIGHT*n-1, 0), BITMAP_WIDTH, MIN(CHUNK_HEIGHT*(n+1)+1, BITMAP_HEIGHT)), \
    RdpSyncLoad(), \
    RdpTextureRectangle1I(TILE0, 0, CHUNK_HEIGHT*(n)*2, 320, CHUNK_HEIGHT*(n+1)*2), \
    RdpTextureRectangle2F(0, CHUNK_HEIGHT*n, 0.25f, 0.5f)

static uint64_t dl_draw_bitmap[] = {
    [0] = RdpSetTexImage(RDP_TILE_FORMAT_I, RDP_TILE_SIZE_8BIT, (uint32_t)TEXTURE_BUFFER, BITMAP_WIDTH),
    RdpSetOtherModes(SOM_CYCLE_2 | SOM_SAMPLE_BILINEAR | SOM_Z_WRITE | SOM_ZSOURCE_PRIM | SOM_RGBDITHER_BAYER),
    RdpSetCombine(RDPQ_COMBINER2(
        (TEX0,0,PRIM,0), (0,0,0,1),
        (NOISE,0,PRIM_ALPHA,COMBINED), (0,0,0,1)
    )),
    RdpSetPrimColor(RGBA32(0x8A, 0x4F, 0xFF, 0x4)),
    RdpSetTile(RDP_TILE_FORMAT_I, RDP_TILE_SIZE_8BIT, BITMAP_WIDTH, 0, TILE0),

    DRAW_BITMAP_CHUNK(0),
    DRAW_BITMAP_CHUNK(1),
    DRAW_BITMAP_CHUNK(2),

    RdpSyncFull(),
};

#define dl_draw_bitmap_cnt  (sizeof(dl_draw_bitmap) / sizeof(uint64_t))

static void fracgen_draw(void)
{
    int tbuf_offset = (framecount & 1) * BITMAP_WIDTH*BITMAP_HEIGHT/2;
    fracgen(tbuf_calc_frame + tbuf_offset, tbuf_offset, BITMAP_WIDTH * BITMAP_HEIGHT/2);

    if ((framecount & 1)) {
        texgen_framecount++;

        uint32_t *udl = UncachedAddr(dl_draw_bitmap);
        udl[1] = (uint32_t)tbuf_calc_frame;

        tbuf_calc_frame = (void*)((uint32_t)tbuf_calc_frame ^ (BITMAP_WIDTH*BITMAP_HEIGHT));
    }

    dp_send(dl_draw_bitmap, dl_draw_bitmap + dl_draw_bitmap_cnt);
}
