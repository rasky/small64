#include <stdint.h>
#include "assets/font.c.inc"
#include "rdp_commands.h"
#include "rdpq_macros.h"

// NOTE: RdpSyncPipe expands to 50 nops, so RdpSetTexImage is command #50
// NOTE: RdpSyncLoad expands to 25 nops, so RdpTexRect is 77
#define DRAW_CHAR() \
    RdpSyncPipe(), \
    RdpSetTexImage(RDP_TILE_FORMAT_IA, RDP_TILE_SIZE_8BIT, NULL, (CHAR_WIDTH+1)/2), \
    RdpLoadTileI(TILE2, 0, 0, (CHAR_WIDTH+1)/2, CHAR_HEIGHT), \
    RdpSyncLoad(), \
    0, \
    RdpTextureRectangle2F(0, 0, 1, 0.5)

static RdpList dl_text[] = {
    RdpSyncPipe(),
    RdpSetOtherModes(SOM_CYCLE_2 | SOM_ALPHA_COMPARE),
    RdpSetCombine(RDPQ_COMBINER2(
        (TEX1,0,0,TEX0),  (PRIM,0,TEX0,TEX1),
        (COMBINED,0,PRIM,0), (0,0,0,COMBINED)
    )),
    RdpSetBlendColor(RGBA32(0, 0, 0, 8)),
    RdpSetTile(RDP_TILE_FORMAT_IA, RDP_TILE_SIZE_4BIT, CHAR_WIDTH, 0, TILE0),
    RdpSetTile(RDP_TILE_FORMAT_IA, RDP_TILE_SIZE_4BIT, CHAR_WIDTH, 0, TILE1),
    RdpSetTile(RDP_TILE_FORMAT_IA, RDP_TILE_SIZE_8BIT, CHAR_WIDTH, 0, TILE2),
    RdpSetTileSizeI(TILE0, 0, 0, CHAR_WIDTH, CHAR_HEIGHT),
    RdpSetTileSizeI(TILE1, 1, 1, CHAR_WIDTH+1, CHAR_HEIGHT+1),
    [69] = RdpSetPrimColor(RGBA32(0, 0, 0, 0)),

    [70] = DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(),
    DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(),
    DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(),
    DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR()
};

uint32_t colors[4] = {
    0xFFFFFF10,     // white
    0xF4D35E10,     // yellow
    0x8A4FFF10,     // purple
    0xFFAA9910,     // melon
};

static void draw_text(uint32_t color, const uint8_t *text, int text_len, int xpos0, int ypos0)
{
    int xpos = xpos0;
    int ypos = ypos0;
    
    uint32_t *w = (uint32_t*)UncachedAddr(dl_text + 70);
    w[-1] = color;

    for (int i=0; i<text_len; i++) {
        if (xpos > 320) break;
        if (xpos >= 0 && text[i] != 0x60) {
            int index = (text[i] & 0b11111);
            w[50*2+1] = (uint32_t)(font - CHAR_SIZE + index * CHAR_SIZE);

            uint32_t xypos = (xpos << 14) | (ypos << 2);
            const int chsize = ((CHAR_WIDTH << 14) | ((CHAR_HEIGHT*2) << 2));
            w[77*2+0] = (0x24 << 24) | (xypos + chsize);
            w[77*2+1] = xypos;
            w += 79*2;
        }
        xpos += (text[i] >> 5) + CHAR_SPACING_OFFSET;
    }

    *w = RdpSyncFull() >> 32;
    dp_send(dl_text, w+2);
    *w = 0; //RdpSyncPipe() >> 32;
}

#if 0
void draw_scroller(uint16_t *FB)
{
    static int xpos0 = 500;

    draw_text(FB, 0xffffffff, phrase_scroller, phrase_scroller_len, xpos0, 200);

    xpos0 -= 2;
}
#endif

DEBUG_NOINLINE
static int draw_intro_setup(void)
{
    static bool visible = false;
    volatile uint32_t* regs = (uint32_t*)0xA4400000;

    int fc = framecount - T_INTRO;
    if (fc < 0) return -1;
    int intro_phidx = fc >> 6;
    if (intro_phidx > 2) {
        vi_reset(2);
        return intro_phidx;
    }

    unsigned int rand = C0_COUNT();
    if ((visible && (rand&0xff) < 24) || fc == 1) {
        visible = false;
        *VI_H_VIDEO = 0x0;
    } else if (!visible && (rand&0xff) < 8*3) {
        visible = true;
        *VI_X_SCALE = 0x140 + ((rand>>8) & 0x7F);
        *VI_Y_SCALE = 0x80 + ((rand>>16) & 0x7F);
        *VI_H_VIDEO = vi_regs_p[9];
    }

    return intro_phidx;
}

static void draw_intro(int phidx)
{
    if (phidx > 2) return;
    draw_text(colors[phidx], phrases+phrases_off[phidx], phrases_off[phidx+1] - phrases_off[phidx], 100, 8);
}

static void draw_credits(void)
{
    static int xpos0[4] = { 120, 90, 120, 120 };
    const int PH_START = 3;

    int fc = framecount - T_CREDITS;
    int phidx = fc >> 7;
    if (phidx > 3) return;

    fc &= 127;
    int xpos = xpos0[phidx];
    if ((fc & 64) == 0) {
        float fcf = (fc & 63) * (1.0f / 63.0f);
        xpos += (400-xpos) * (1-fcf*fcf*fcf*fcf*fcf);
    } 

    uint32_t color = (fc > 0xD0/2 && fc & 2) ? colors[1] : colors[0];

    phidx += PH_START;
    int off = phrases_off[phidx];
    draw_text(color, phrases+off, phrases_off[phidx+1] - off, xpos, 200);
}
