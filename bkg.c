#include <stdint.h>
#include "rdp_commands.h"
#include "rdpq_macros.h"

#define TILE0    0
#define TILE1    1

__attribute__((aligned(8)))
uint8_t checkerboard[] = {
    0xBE, 0xBE, 0xBE, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xBE, 0xBE, 0xBE, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xBE, 0xBE, 0xBE, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xBE, 0xBE, 0xBE, 0xBE, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xBE, 0xBE, 0xBE, 0xBE, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xBE, 0xBE, 0xBE, 0xBE, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xBE, 0xBE, 0xBE, 0xBE, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xBE, 0xBE, 0xBE, 0xBE, 
};

__attribute__((aligned(8)))
uint8_t gradient[8] = {
    0xFF, 0xEE, 0xDD, 0xBB, 0x99, 0x77, 0x77, 0x77,
};

uint64_t dl_bkg[] = {
    [0] = RdpSyncPipe(),
    [1] = RdpSetColorImage(RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_16BIT, 320, 0),
    [2] = RdpSetClippingI(0, 0, 320, 240),
    [3] = RdpSetZImage(Z_BUFFER),
#if 0
    [3] = RdpSetOtherModes(SOM_CYCLE_1),
    [4] = RdpSetPrimColor(RGBA32(0xCB,0x2B,0x40,0xFF)),
    [5] = RdpSetCombine(RDPQ_COMBINER_FLAT),
    [6] = RdpFillRectangleI(0, 0, 320, 240),
#endif
    [10] = RdpSyncPipe(),
           RdpSetPrimColor(RGBA32((0xFF*0.55), 0xCC*0.55, 0xAA*0.55, 0xFF)),
           RdpSetCombine(RDPQ_COMBINER2(
                (TEX1,0,TEX0,0), (0,0,0,1),
                (COMBINED,0,PRIM,0), (0,0,0,1)
           )),
           RdpSetOtherModes(SOM_CYCLE_2 | SOM_RGBDITHER_BAYER | SOM_ALPHADITHER_BAYER | SOM_SAMPLE_BILINEAR),
    [20] = RdpSetTexImage(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_8BIT, NULL, 8),
           RdpSetTile(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_8BIT, 8, 0, TILE0) | 
                RdpSetTile_Mask(3, 3),
           RdpLoadTileI(TILE0, 0, 0, 8, 8),
           RdpSyncPipe(),
    [30] = RdpSetTexImage(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_8BIT, NULL, 1),
           RdpSetTile(RDP_TILE_FORMAT_INDEX, RDP_TILE_SIZE_8BIT, 8, 0x200, TILE1) | 
                RdpSetTile_Mask(0, 3) | RdpSetTile_Scale(0, 1) | RdpSetTile_Mirror(0, 1),
           RdpLoadTileI(TILE1, 0, 0, 1, 8),
           RdpSyncPipe(),

    [39] = RdpSetTileSizeI(TILE0, 0, 0, 8, 8),
    [40] = RdpTextureRectangle1I(TILE0, 0, 0, 320, 240),
    [41] = RdpTextureRectangle2F(0, 0, 0.25, 0.25),

    // setup for the following 3D draw
    [42] = RdpSyncPipe(),
           RdpSyncTile(),
           RdpSetCombine(RDPQ_COMBINER2(
              // inverted fresnel, scale down to 0 by amount of fresnel
              (TEX1,TEX0,SHADE_ALPHA, TEX0), (0,0,0,1),
              // color in cycle 1 with PRIM, then apply specular
              (COMBINED,0,PRIM,SHADE),     (0,0,0,1)
           )),

    [45] = RdpSetTexImage(RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_32BIT, NULL, 8),
           RdpSetTile(RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_32BIT, 8, 0, TILE0) | 
                RdpSetTile_Mask(3, 3) | RdpSetTile_Scale(3, 3),
           RdpLoadTileI(TILE0, 0, 0, 8, 8),
    [49] = RdpSetTexImage(RDP_TILE_FORMAT_I, RDP_TILE_SIZE_8BIT, NULL, 8),
           RdpSetTile(RDP_TILE_FORMAT_I, RDP_TILE_SIZE_8BIT, 8, 0x400, TILE1) | 
                RdpSetTile_Mask(3, 3) | RdpSetTile_Scale(3, -1),
           RdpLoadTileI(TILE1, 0, 0, 8, 8),
           RdpSetOtherModes(SOM_CYCLE_2 | SOM_Z_COMPARE | SOM_Z_WRITE | SOM_SAMPLE_BILINEAR),
           RdpSetPrimColor(RGBA32(0xFF, 0xAA, 0x99, 0xFF)),
    [64] = RdpSyncFull(),
};


#define dl_bkg_cnt  (sizeof(dl_bkg) / sizeof(uint64_t))

void draw_bkg(void)
{
    uint64_t *uncached_dl_bkg = (uint64_t*)((uint32_t)dl_bkg | 0xA0000000);

    uncached_dl_bkg[1] = (uncached_dl_bkg[1] & 0xFFFFFFFF00000000ull) | (uint32_t)vi_buffer_draw;

    // FIXME: these should be unnecessary
    uncached_dl_bkg[20] |= (uint32_t)checkerboard;
    uncached_dl_bkg[30] |= (uint32_t)gradient;
    uncached_dl_bkg[45] |= (uint32_t)checkerboard + 32;
    uncached_dl_bkg[49] |= (uint32_t)bbsong_data + 16;

    static int count = 0;
    if (++count == 3) {
        uncached_dl_bkg[39] += (1ull << 44) | (1ull << 32) | (1ull << 12) | (1ull << 0);
        count = 0;
    }

    dp_send(dl_bkg, dl_bkg+dl_bkg_cnt);
    dp_wait();
}
