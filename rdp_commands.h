#ifndef RDP_COMMANDS_H
#define RDP_COMMANDS_H

#define RDP_TILE_FORMAT_RGBA  0
#define RDP_TILE_FORMAT_YUV   1
#define RDP_TILE_FORMAT_INDEX 2
#define RDP_TILE_FORMAT_IA    3
#define RDP_TILE_FORMAT_I     4

#define RDP_TILE_SIZE_4BIT  0
#define RDP_TILE_SIZE_8BIT  1
#define RDP_TILE_SIZE_16BIT 2
#define RDP_TILE_SIZE_32BIT 3

#define RDP_COLOR16(r,g,b,a) (uint32_t)(((r)<<11)|((g)<<6)|((b)<<1)|(a))
#define RDP_COLOR32(r,g,b,a) (uint32_t)(((r)<<24)|((g)<<16)|((b)<<8)|(a))

#define TILE0       0
#define TILE1       1
#define TILE2       2
#define TILE3       3
#define TILE4       4
#define TILE5       5
#define TILE6       6
#define TILE7       7

#define RdpList __attribute__((aligned(16))) uint64_t

// When compiling C/C++ code, 64-bit immediate operands require explicit
// casting to a 64-bit type
#ifdef __ASSEMBLER__
#define cast64(x) (x)
#else
#include <stdint.h>
#define cast64(x) (uint64_t)(x)
#endif

#define RdpSetClippingFX(x0,y0,x1,y1) \
    ((cast64(0x2D))<<56 | (cast64(x0)<<44) | (cast64(y0)<<32) | ((x1)<<12) | ((y1)<<0))
#define RdpSetClippingI(x0,y0,x1,y1) RdpSetClippingFX((x0)<<2, (y0)<<2, (x1)<<2, (y1)<<2)
#define RdpSetClippingF(x0,y0,x1,y1) RdpSetClippingFX((int)((x0)*4), (int)((y0)*4), (int)((x1)*4), (int)((y1)*4))

#define RdpSetConvert(k0,k1,k2,k3,k4,k5) \
    ((cast64(0x2C)<<56) | ((cast64((k0))&0x1FF)<<45) | ((cast64((k1))&0x1FF)<<36) | ((cast64((k2))&0x1FF)<<27) | ((cast64((k3))&0x1FF)<<18) | ((cast64((k4))&0x1FF)<<9) | ((cast64((k5))&0x1FF)<<0))

#define RdpSetTile(fmt, size, line, addr, tidx) \
    ((cast64(0x35)<<56) | (cast64((fmt)) << 53) | (cast64((size)) << 51) | (cast64(((line)+7)/8) << 41) | (cast64((addr)/8) << 32) | ((tidx) << 24))

#define RdpSetTile_Mask(masks, maskt)       (((masks)<<4) | ((maskt)<<14))
#define RdpSetTile_Scale(scales, scalet)    ( (((scales)<<0) & 0b1111) | (((scalet)<<10) & (0b1111 << 10)) )
#define RdpSetTile_Mirror(mirrors, mirrort) (((mirrors)<<8) | ((mirrort)<<18))

#define RdpSetTexImage(fmt, size, addr, width) \
    ((cast64(0x3D)<<56) | ((uint32_t)(addr)) | (cast64(((width))-1)<<32) | (cast64((fmt))<<53) | (cast64((size))<<51))

#define RdpLoadTileFX(tidx,s0,t0,s1,t1) \
    ((cast64(0x34)<<56) | (cast64((tidx))<<24) | (cast64((s0))<<44) | (cast64((t0))<<32) | ((s1)<<12) | ((t1)<<0))
#define RdpLoadTileI(tidx,s0,t0,s1,t1) RdpLoadTileFX(tidx, (s0)<<2, (t0)<<2, (s1)<<2, (t1)<<2)

#define RdpLoadTlut(tidx, lowidx, highidx) \
    ((cast64(0x30)<<56) | (cast64(tidx) << 24) | (cast64(lowidx)<<46) | (cast64(highidx)<<14))

#define RdpLoadBlock(tidx, s0, t0, num_texels, pitch) \
    ((cast64(0x33)<<56) | (cast64(tidx) << 24) | (cast64(s0)<<44) | (cast64(t0)<<32) | \
     (cast64((num_texels)-1)<<12) | (cast64((2048 + (pitch)/8 - 1) / ((pitch)/8))<<0))

#define RdpSetTileSizeFX(tidx,s0,t0,s1,t1) \
    ((cast64(0x32)<<56) | ((tidx)<<24) | (cast64(s0)<<44) | (cast64(t0)<<32) | ((s1)<<12) | ((t1)<<0))
#define RdpSetTileSizeI(tidx,s0,t0,s1,t1) \
    RdpSetTileSizeFX(tidx, (s0)<<2, (t0)<<2, (s1)<<2, (t1)<<2)

#define RdpTextureRectangle1FX(tidx,x0,y0,x1,y1) \
    ((cast64(0x24)<<56) | (cast64((x1)&0xFFF)<<44) | (cast64((y1)&0xFFF)<<32) | ((tidx)<<24) | (((x0)&0xFFF)<<12) | (((y0)&0xFFF)<<0))
#define RdpTextureRectangle1I(tidx,x0,y0,x1,y1) \
    RdpTextureRectangle1FX(tidx, (x0)<<2, (y0)<<2, (x1)<<2, (y1)<<2)
#define RdpTextureRectangle1F(tidx,x0,y0,x1,y1) \
    RdpTextureRectangle1FX(tidx, (int32_t)((x0)*4.f), (int32_t)((y0)*4.f), (int32_t)((x1)*4.f), (int32_t)((y1)*4.f))

#define RdpTextureRectangle2FX(s,t,ds,dt) \
    ((cast64((s)&0xFFFF)<<48) | (cast64((t)&0xFFFF)<<32) | (cast64((ds)&0xFFFF)<<16) | (cast64((dt)&0xFFFF)<<0))
#define RdpTextureRectangle2I(s,t,ds,dt) \
    RdpTextureRectangle2FX((s)<<5, (t)<<5, (ds)<<10, (dt)<<10)
#define RdpTextureRectangle2F(s,t,ds,dt) \
    RdpTextureRectangle2FX((int32_t)((s)*32.f), (int32_t)((t)*32.f), (int32_t)((ds)*1024.f), (int32_t)((dt)*1024.f))

#define RdpSetColorImage(fmt, size, width, addr) \
    ((cast64(0x3f)<<56) | (cast64(fmt)<<53) | (cast64(size)<<51) | ((cast64(width)-1)<<32) | ((addr)<<0))
#define RdpSetZImage(addr) \
    ((cast64(0x3e)<<56) | ((uint32_t)(addr)<<0))

#define RdpFillRectangleFX(x0,y0,x1,y1) \
    ((cast64(0x36)<<56) | ((x0)<<12) | ((y0)<<0) | (cast64(x1)<<44) | (cast64(y1)<<32))
#define RdpFillRectangleI(x0,y0,x1,y1) RdpFillRectangleFX((x0)<<2, (y0)<<2, (x1)<<2, (y1)<<2)
#define RdpFillRectangleF(x0,y0,x1,y1) RdpFillRectangleFX((int)((x0)*4), (int)((y0)*4), (int)((x1)*4), (int)((y1)*4))

#define RdpSetFillColor16(color) \
    (((cast64(0x37))<<56) | (cast64(color)<<16) | (color))

#define RdpSetFillColor(color) \
    (((cast64(0x37))<<56) | (uint32_t)(color))

#define RdpSetPrimColor(color) \
    (((cast64(0x3a))<<56) | (uint32_t)(color))

#define RdpSetPrimDepth(depth) \
    (((cast64(0x2e))<<56) | ((uint32_t)(depth) << 16))

#define RdpSetEnvColor(color) \
    (((cast64(0x3b))<<56) | (uint32_t)(color))

#define RdpSetBlendColor(color) \
    (((cast64(0x39))<<56) | (uint32_t)(color))

#define RdpSetFogColor(color) \
    (((cast64(0x38))<<56) | (uint32_t)(color))

// RDP command to configure the color combiner. Use rdpq_macros
// like RDPQ_COMBINER1 or RDPQ_COMBINER2 to create the argument.
#define RdpSetCombine(cc) \
    ((cast64(0x3C)<<56) | cc)

#define SOM_CYCLE_1    ((cast64(0))<<52)
#define SOM_CYCLE_2    ((cast64(1))<<52)
#define SOM_CYCLE_COPY ((cast64(2))<<52)
#define SOM_CYCLE_FILL ((cast64(3))<<52)

#define SOM_TEXTURE_DETAIL     (cast64(1)<<50)
#define SOM_TEXTURE_SHARPEN    (cast64(1)<<49)

#define SOM_ENABLE_TLUT_RGB16  (cast64(2)<<46)
#define SOM_ENABLE_TLUT_I88    (cast64(3)<<46)

#define SOM_SAMPLE_1X1         (cast64(0)<<45)
#define SOM_SAMPLE_2X2         (cast64(1)<<45)
#define SOM_MIDTEXEL           (cast64(1)<<44)

#define SOM_TC_FILTER          (cast64(0)<<41)  // NOTE: this values are bit-inverted, so that they end up with a good default
#define SOM_TC_FILTERCONV      (cast64(3)<<41)
#define SOM_TC_CONV            (cast64(6)<<41)

#define SOM_RGBDITHER_SQUARE   ((cast64(0))<<38)
#define SOM_RGBDITHER_BAYER    ((cast64(1))<<38)
#define SOM_RGBDITHER_NOISE    ((cast64(2))<<38)
#define SOM_RGBDITHER_NONE     ((cast64(3))<<38)

#define SOM_ALPHADITHER_SQUARE ((cast64(0))<<36)
#define SOM_ALPHADITHER_BAYER  ((cast64(1))<<36)
#define SOM_ALPHADITHER_NOISE  ((cast64(2))<<36)
#define SOM_ALPHADITHER_NONE   ((cast64(3))<<36)

#define SOM_BLENDING           ((cast64(1))<<14)
#define SOM_Z_WRITE            ((cast64(1))<<5)
#define SOM_Z_COMPARE          ((cast64(1))<<4)
#define SOM_ALPHA_COMPARE      ((cast64(1))<<0)

#define SOM_READ_ENABLE                 ((cast64(1)) << 6)
#define SOM_AA_ENABLE                   ((cast64(1)) << 3)
#define SOM_COVERAGE_DEST_CLAMP         ((cast64(0)) << 8)
#define SOM_COVERAGE_DEST_WRAP          ((cast64(1)) << 8)
#define SOM_COVERAGE_DEST_ZAP           ((cast64(2)) << 8)
#define SOM_COVERAGE_DEST_SAVE          ((cast64(3)) << 8)
#define SOM_COLOR_ON_COVERAGE           ((cast64(1)) << 7)


#define RdpSetOtherModes(som_flags) \
    ((cast64(0x2f)<<56) | ((som_flags) ^ (cast64(6)<<41)))

#define RdpSyncFull() \
    (cast64(0x29)<<56)

#if 1

// RDP syncs are just wait states. For a 4K, it's better to emit
// sequences of zeros that will be compressed rather than using the specific
// opcode.
#define RdpSyncLoad() \
    0,0,0,0,0,0,0,0, \
    0,0,0,0,0,0,0,0, \
    0,0,0,0,0,0,0,0,0
#define RdpSyncPipe() \
    RdpSyncLoad(), \
    RdpSyncLoad()
#define RdpSyncTile() \
    RdpSyncLoad(), \
    0,0,0,0,0,0,0,0
#else
#define RdpSyncLoad() \
    (cast64(0x26)<<56)
#define RdpSyncPipe() \
    (cast64(0x27)<<56)
#define RdpSyncTile() \
    (cast64(0x28)<<56)
#endif



#endif /* RDP_COMMANDS_H */
