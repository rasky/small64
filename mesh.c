#include "minidragon.h"
#include "minimath.h"
#include <stdint.h>
#include "rdp_commands.h"

#define MIN(a,b) ((a) < (b) ? (a) : (b))

typedef struct {
  int8_t pos[3];
  int8_t _padding0;
  int8_t normal[3];
  int8_t _padding1;
} u3d_vertex;

static float xangle = MM_PI/8;
static float yangle = MM_PI/4;

uint32_t mesh(void)
{
    // Number of segments for the major (u) and minor (v) circles.
    const int numMajor = 32; // Segments along the torus' main circle.
    const int numMinor = 16; // Segments along the tube (minor circle).
    const int stepMajor = 0x100 / numMajor;
    const int stepMinor = 0x100 / numMinor;

    const int uEnd = stepMajor * numMajor;
    const int vEnd = stepMinor * numMinor;

    // Torus parameters: major radius (R) and minor radius (r).
    const int R = 64;

    const int shiftXY = 7;
    const int shiftZ = 2;

    static u3d_vertex *vtx = VERTEX_BUFFER;
    if (vtx == VERTEX_BUFFER) {

        // Loop over the torus patches.
        for(int u0 = 0; u0 != uEnd; u0 += stepMajor)
        {
            int u1 = u0 + stepMajor;
            int8_t cosV1, sinV1;
            int8_t cosV0 = 0x7F;
            int8_t sinV0 = 0;

            for(int v0 = 0; v0 != vEnd; v0 += stepMinor)
            {
                // Calculate the current and next angle for the minor circle.
                cosV1 = mm_cos_s8(v0 + stepMinor);
                sinV1 = mm_sin_s8(v0 + stepMinor);

                // Compute the four vertices for the current quad.
                int rcv0 = R + (cosV0 >> shiftZ);
                int rcv1 = R + (cosV1 >> shiftZ);

                vtx[0].pos[0] = (rcv0 * mm_cos_s8(u0)) >> shiftXY;
                vtx[0].pos[1] = (rcv0 * mm_sin_s8(u0)) >> shiftXY;

                vtx[1].pos[0] = (rcv0 * mm_cos_s8(u1)) >> shiftXY;
                vtx[1].pos[1] = (rcv0 * mm_sin_s8(u1)) >> shiftXY;

                vtx[2].pos[0] = (rcv1 * mm_cos_s8(u1)) >> shiftXY;
                vtx[2].pos[1] = (rcv1 * mm_sin_s8(u1)) >> shiftXY;

                vtx[3].pos[0] = (rcv1 * mm_cos_s8(u0)) >> shiftXY;
                vtx[3].pos[1] = (rcv1 * mm_sin_s8(u0)) >> shiftXY;

                vtx[0].pos[2] = sinV0 >> shiftZ;
                vtx[1].pos[2] = sinV0 >> shiftZ;
                vtx[2].pos[2] = sinV1 >> shiftZ;
                vtx[3].pos[2] = sinV1 >> shiftZ;

                vtx[0].normal[0] = (mm_cos_s8(u0) * cosV0) >> shiftXY;
                vtx[0].normal[1] = (mm_sin_s8(u0) * cosV0) >> shiftXY;
                vtx[0].normal[2] = sinV0;

                vtx[1].normal[0] = (mm_cos_s8(u1) * cosV0) >> shiftXY;
                vtx[1].normal[1] = (mm_sin_s8(u1) * cosV0) >> shiftXY;
                vtx[1].normal[2] = sinV0;

                vtx[2].normal[0] = (mm_cos_s8(u1) * cosV1) >> shiftXY;
                vtx[2].normal[1] = (mm_sin_s8(u1) * cosV1) >> shiftXY;
                vtx[2].normal[2] = sinV1;

                vtx[3].normal[0] = (mm_cos_s8(u0) * cosV1) >> shiftXY;
                vtx[3].normal[1] = (mm_sin_s8(u0) * cosV1) >> shiftXY;
                vtx[3].normal[2] = sinV1;

                vtx[4] = vtx[0];
                vtx[5] = vtx[2];
                vtx += 6;

                cosV0 = cosV1;
                sinV0 = sinV1;
            }
        }
    }

   return (uint32_t)vtx;
}

static RdpList dl_setup_3d[] = {
    [0] = RdpSetEnvColor(RGBA32(0x00, 0x00, 0x00, 0x1)),
    [1] = RdpSetTexImage(RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_32BIT, 0x0a00, 8),
           RdpSetTile(RDP_TILE_FORMAT_RGBA, RDP_TILE_SIZE_32BIT, 8, 0, TILE0) |
                RdpSetTile_Mask(3, 3) | RdpSetTile_Scale(3, 3),
           RdpLoadTileI(TILE0, 0, 0, 8, 8),

           RdpSetTile(RDP_TILE_FORMAT_I, RDP_TILE_SIZE_8BIT, 8, 0x10, TILE1) |
                RdpSetTile_Mask(3, 3) | RdpSetTile_Scale(3, -1),
           RdpSetOtherModes(SOM_CYCLE_2 | SOM_Z_COMPARE | SOM_Z_WRITE | SOM_ZSOURCE_PIXEL | SOM_SAMPLE_BILINEAR | SOM_ALPHACOMPARE_NOISE),
           RdpSetPrimColor(RGBA32(0xB0, 0xA0, 0x90, 0xFF)),
           RdpSetCombine(RDPQ_COMBINER2(
            // inverted fresnel, scale down to 0 by amount of fresnel
            (TEX1,TEX0,SHADE_ALPHA, TEX0), (0,0,0,ENV),
            // color in cycle 1 with PRIM, then apply specular
            (COMBINED,0,PRIM,SHADE),     (0,0,0,ENV)
         )),
         RdpSyncFull()
};

#define dl_setup_3d_cnt  (sizeof(dl_setup_3d) / sizeof(uint64_t))

static void setup_3d(void)
{
    int torus_fade = MIN((framecount-T_MESH)<<2, 0xFF);
    uint8_t *udl = (uint8_t*)((uint32_t)dl_setup_3d | 0xA0000000);

    udl[7] = torus_fade;

    dp_send(dl_setup_3d, dl_setup_3d+dl_setup_3d_cnt);
}

static void mesh_draw_async(void)
{
    setup_3d();

    xangle += 0.01f;
    yangle += 0.015f;
    
    mesh();
    ucode_set_srt(1.0f, (float[]){xangle, yangle, 0.0f}, 160<<2, 120<<2);

    *DP_STATUS = DP_WSTATUS_SET_XBUS;
    *DP_START = 0x30; // @TODO: why do i have to set both here? (hangs otherwise)
    *DP_END = 0x30;

    if (framecount > T_ANIMATE) {
        static float dispTimer = -3;
        float dispFactor = mm_sinf(__builtin_fmaxf(dispTimer, 0));
        ucode_set_displace(dispFactor * 0x7FFF);
        if(dispTimer > MM_PI*2)dispTimer = -2;
        dispTimer += 0.01f;
    }

    ucode_run();

}

static void mesh_draw_wait(void)
{
    ucode_sync();
    dp_wait();
    *DP_STATUS = DP_WSTATUS_CLR_XBUS;
}
