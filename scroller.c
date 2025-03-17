#include <stdint.h>
#include "assets/font.c.inc"
#include "rdp_commands.h"
#include "rdpq_macros.h"

#define CHAR_SIZE   (CHAR_WIDTH * CHAR_HEIGHT)

#define DRAW_CHAR() \
    RdpSyncPipe(), \
    RdpSetTexImage(RDP_TILE_FORMAT_IA, RDP_TILE_SIZE_8BIT, NULL, CHAR_WIDTH), \
    RdpSetTile(RDP_TILE_FORMAT_IA, RDP_TILE_SIZE_8BIT, CHAR_WIDTH, 0, TILE0), \
    RdpLoadTileI(TILE0, 0, 0, CHAR_WIDTH, CHAR_HEIGHT), \
    RdpSyncLoad(), \
    RdpSetPrimColor(RGBA32(0x0, 0x0, 0x0, 0xFF)), \
    RdpTextureRectangle1I(TILE0, 0, 0, 0, 0), \
    RdpTextureRectangle2F(0, 0, 1, 0.5), \
    RdpSetPrimColor(RGBA32(0xFF, 0xFF, 0xFF, 0xFF)), \
    RdpTextureRectangle1I(TILE0, 0, 0, 0, 0), \
    RdpTextureRectangle2F(0, 0, 1, 0.5)

static uint64_t dl_text[] = {
    RdpSyncPipe(),
    RdpSetOtherModes(SOM_CYCLE_1 | SOM_ALPHA_COMPARE),
    RdpSetCombine(RDPQ_COMBINER_TEX_FLAT),
    RdpSetBlendColor(RGBA32(0, 0, 0, 8)),

    [16] = DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(),
    DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(),
    DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(),
    DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(),
    DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(),
    DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(), DRAW_CHAR(),
};

void draw_scroller(uint16_t *FB)
{
    static int xpos0 = 500;

    int xpos = xpos0;
    int ypos = 150;
    const uint8_t *text = phrase_bin;

    uint32_t *w = (uint32_t*)UncachedAddr(dl_text + 16);

    for (int i=0; i<phrase_bin_len; i++) {
        if (xpos > 320) break;
        if (xpos >= 0 && text[i] != 0) {
            int index = text[i]-1;
            w[3] = (uint32_t)(font + index * CHAR_SIZE);

            uint32_t xypos = (xpos << 14) | (ypos << 2);
            const int shadowoff = ((2 << 14) | (2 << 2));
            const int chsize = ((CHAR_WIDTH << 14) | ((CHAR_HEIGHT*2) << 2));
            w[18] = (0x24 << 24) | (xypos + chsize);
            w[19] = xypos;
            xypos += shadowoff;
            w[12] = (0x24 << 24) | (xypos + chsize);
            w[13] = xypos;
            w += 22;
        }
        xpos += widths_bin[text[i]]+1; 
    }

    xpos0 -= 2;

    *w = RdpSyncFull() >> 32;
    dp_send(dl_text, w+2);
    dp_wait();
    *w = RdpSyncPipe() >> 32;
}
