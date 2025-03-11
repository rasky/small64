#include "minimath.h"
#include <stdint.h>
#include "rdp_commands.h"

#define tracef(s, ...)  ({ })

#define FLT_MIN 1.175494351e-38f
#define FLT_MAX 3.402823466e+38f

#define _carg(value, mask, shift) (((uint32_t)((value) & (mask))) << (shift))


enum {
    RDPQ_CMD_TRI                        = 0x08,
    RDPQ_CMD_TRI_ZBUF                   = 0x09,
    RDPQ_CMD_TRI_TEX                    = 0x0A,
    RDPQ_CMD_TRI_TEX_ZBUF               = 0x0B,
    RDPQ_CMD_TRI_SHADE                  = 0x0C,
    RDPQ_CMD_TRI_SHADE_ZBUF             = 0x0D,
    RDPQ_CMD_TRI_SHADE_TEX              = 0x0E,
    RDPQ_CMD_TRI_SHADE_TEX_ZBUF         = 0x0F,
};

/** @brief Converts a float to a s16.16 fixed point number */
static int32_t float_to_s16_16(float f)
{
    // Currently the float must be clamped to this range because
    // otherwise the trunc.w.s instruction can potentially trigger
    // an unimplemented operation exception due to integer overflow.
    // TODO: maybe handle the exception? Clamp the value in the exception handler?
    if (f >= 32768.f) {
        return 0x7FFFFFFF;
    }

    if (f < -32768.f) {
        return 0x80000000;
    }

    return mm_floorf(f * 65536.f);
}

typedef struct {
  uint16_t pos[3];
  uint16_t color;
} u3d_vertex;

#define VERTEX_SIZE     3       //x,y,z
#define VTX_POS_OFFSET  0
#define VTX_Z_OFFSET    2
#define VERTEX_RDP      RDPQ_CMD_TRI_ZBUF

/** @brief Precomputed information about edges and slopes. */
typedef struct {
    float hx;               ///< High edge (X)
    float hy;               ///< High edge (Y)
    float mx;               ///< Middle edge (X)
    float my;               ///< Middle edge (Y)
    float fy;               ///< Fractional part of Y1 (top vertex)
    float ish;              ///< Inverse slope of higher edge
    float attr_factor;      ///< Inverse triangle normal (used to calculate gradients)
} rdpq_tri_edge_data_t;

static uint32_t* __rdpq_write_edge_coeffs(uint32_t *w, rdpq_tri_edge_data_t *data, uint8_t tile, uint8_t mipmaps, float cull_side, const float *v1, const float *v2, const float *v3)
{
    const float x1 = v1[0];
    const float x2 = v2[0];
    const float x3 = v3[0];
    const float y1 = mm_floorf(v1[1]*4)/4;
    const float y2 = mm_floorf(v2[1]*4)/4;
    const float y3 = mm_floorf(v3[1]*4)/4;

    const float to_fixed_11_2 = 4.0f;
    int32_t y1f = CLAMP((int32_t)mm_floorf(v1[1]*to_fixed_11_2), -4096*4, 4095*4);
    int32_t y2f = CLAMP((int32_t)mm_floorf(v2[1]*to_fixed_11_2), -4096*4, 4095*4);
    int32_t y3f = CLAMP((int32_t)mm_floorf(v3[1]*to_fixed_11_2), -4096*4, 4095*4);

    data->hx = x3 - x1;
    data->hy = y3 - y1;
    data->mx = x2 - x1;
    data->my = y2 - y1;
    float lx = x3 - x2;
    float ly = y3 - y2;

    const float nz = (data->hx*data->my) - (data->hy*data->mx);
    data->attr_factor = (fabsf(nz) > FLT_MIN) ? (-1.0f / nz) : 0;
    const uint32_t lft = nz < 0;
    if (nz*cull_side < 0) return NULL;

    data->ish = (fabsf(data->hy) > FLT_MIN) ? (data->hx / data->hy) : 0;
    float ism = (fabsf(data->my) > FLT_MIN) ? (data->mx / data->my) : 0;
    float isl = (fabsf(ly) > FLT_MIN) ? (lx / ly) : 0;
    data->fy = mm_floorf(y1) - y1;

    const float xh = x1 + data->fy * data->ish;
    const float xm = x1 + data->fy * ism;
    const float xl = x2;

    *w++ = _carg(VERTEX_RDP, 0x3F, 24) | _carg(lft, 0x1, 23) | _carg(mipmaps ? mipmaps-1 : 0, 0x7, 19) | _carg(tile, 0x7, 16) | _carg(y3f, 0x3FFF, 0);
    *w++ = _carg(y2f, 0x3FFF, 16) | _carg(y1f, 0x3FFF, 0);
    *w++ = float_to_s16_16(xl);
    *w++ = float_to_s16_16(isl);
    *w++ = float_to_s16_16(xh);
    *w++ = float_to_s16_16(data->ish);
    *w++ = float_to_s16_16(xm);
    *w++ = float_to_s16_16(ism);

    tracef("x1:  %f (%08lx)\n", x1, (int32_t)(x1 * 4.0f));
    tracef("x2:  %f (%08lx)\n", x2, (int32_t)(x2 * 4.0f));
    tracef("x3:  %f (%08lx)\n", x3, (int32_t)(x3 * 4.0f));
    tracef("y1:  %f (%08lx)\n", y1, (int32_t)(y1 * 4.0f));
    tracef("y2:  %f (%08lx)\n", y2, (int32_t)(y2 * 4.0f));
    tracef("y3:  %f (%08lx)\n", y3, (int32_t)(y3 * 4.0f));

    tracef("hx:  %f (%08lx)\n", data->hx, (int32_t)(data->hx * 4.0f));
    tracef("hy:  %f (%08lx)\n", data->hy, (int32_t)(data->hy * 4.0f));
    tracef("mx:  %f (%08lx)\n", data->mx, (int32_t)(data->mx * 4.0f));
    tracef("my:  %f (%08lx)\n", data->my, (int32_t)(data->my * 4.0f));
    tracef("lx:  %f (%08lx)\n", lx, (int32_t)(lx * 4.0f));
    tracef("ly:  %f (%08lx)\n", ly, (int32_t)(ly * 4.0f));

    tracef("p1: %f (%08lx)\n", (data->hx*data->my), (int32_t)(data->hx*data->my*16.0f));
    tracef("p2: %f (%08lx)\n", (data->hy*data->mx), (int32_t)(data->hy*data->mx*16.0f));
    tracef("nz: %f (%08lx)\n", nz, (int32_t)(nz * 16.0f));
    tracef("-nz: %f (%08lx)\n", -nz, -(int32_t)(nz * 16.0f));
    tracef("inv_nz: %f (%08lx)\n", data->attr_factor, (int32_t)(data->attr_factor * 65536.0f / 2.0f / 16.0f));
    
    tracef("fy:  %f (%08lx)\n", data->fy, (int32_t)(data->fy * 65536.0f));
    tracef("ish: %f (%08lx)\n", data->ish, (int32_t)(data->ish * 65536.0f));
    tracef("ism: %f (%08lx)\n", ism, (int32_t)(ism * 65536.0f));
    tracef("isl: %f (%08lx)\n", isl, (int32_t)(isl * 65536.0f));

    tracef("xh: %f (%08lx)\n", xh, (int32_t)(xh * 65536.0f));
    tracef("xm: %f (%08lx)\n", xm, (int32_t)(xm * 65536.0f));
    tracef("xl: %f (%08lx)\n", xl, (int32_t)(xl * 65536.0f));

    return w;
}

__attribute__((always_inline))
static inline uint32_t* __rdpq_write_shade_coeffs(uint32_t *w, rdpq_tri_edge_data_t *data, const float *v1, const float *v2, const float *v3)
{
    const float mr = (v2[0] - v1[0]) * 255.f;
    const float mg = (v2[1] - v1[1]) * 255.f;
    const float mb = (v2[2] - v1[2]) * 255.f;
    const float ma = (v2[3] - v1[3]) * 255.f;
    const float hr = (v3[0] - v1[0]) * 255.f;
    const float hg = (v3[1] - v1[1]) * 255.f;
    const float hb = (v3[2] - v1[2]) * 255.f;
    const float ha = (v3[3] - v1[3]) * 255.f;

    const float nxR = data->hy*mr - data->my*hr;
    const float nxG = data->hy*mg - data->my*hg;
    const float nxB = data->hy*mb - data->my*hb;
    const float nxA = data->hy*ma - data->my*ha;
    const float nyR = data->mx*hr - data->hx*mr;
    const float nyG = data->mx*hg - data->hx*mg;
    const float nyB = data->mx*hb - data->hx*mb;
    const float nyA = data->mx*ha - data->hx*ma;

    const float DrDx = nxR * data->attr_factor;
    const float DgDx = nxG * data->attr_factor;
    const float DbDx = nxB * data->attr_factor;
    const float DaDx = nxA * data->attr_factor;
    const float DrDy = nyR * data->attr_factor;
    const float DgDy = nyG * data->attr_factor;
    const float DbDy = nyB * data->attr_factor;
    const float DaDy = nyA * data->attr_factor;

    const float DrDe = DrDy + DrDx * data->ish;
    const float DgDe = DgDy + DgDx * data->ish;
    const float DbDe = DbDy + DbDx * data->ish;
    const float DaDe = DaDy + DaDx * data->ish;

    const int32_t final_r = float_to_s16_16(v1[0] * 255.f + data->fy * DrDe);
    const int32_t final_g = float_to_s16_16(v1[1] * 255.f + data->fy * DgDe);
    const int32_t final_b = float_to_s16_16(v1[2] * 255.f + data->fy * DbDe);
    const int32_t final_a = float_to_s16_16(v1[3] * 255.f + data->fy * DaDe);

    const int32_t DrDx_fixed = float_to_s16_16(DrDx);
    const int32_t DgDx_fixed = float_to_s16_16(DgDx);
    const int32_t DbDx_fixed = float_to_s16_16(DbDx);
    const int32_t DaDx_fixed = float_to_s16_16(DaDx);

    const int32_t DrDe_fixed = float_to_s16_16(DrDe);
    const int32_t DgDe_fixed = float_to_s16_16(DgDe);
    const int32_t DbDe_fixed = float_to_s16_16(DbDe);
    const int32_t DaDe_fixed = float_to_s16_16(DaDe);

    const int32_t DrDy_fixed = float_to_s16_16(DrDy);
    const int32_t DgDy_fixed = float_to_s16_16(DgDy);
    const int32_t DbDy_fixed = float_to_s16_16(DbDy);
    const int32_t DaDy_fixed = float_to_s16_16(DaDy);

    *w++ = (final_r&0xffff0000) | (0xffff&(final_g>>16));
    *w++ = (final_b&0xffff0000) | (0xffff&(final_a>>16));
    *w++ = (DrDx_fixed&0xffff0000) | (0xffff&(DgDx_fixed>>16));
    *w++ = (DbDx_fixed&0xffff0000) | (0xffff&(DaDx_fixed>>16));
    *w++ = (final_r<<16) | (final_g&0xffff);
    *w++ = (final_b<<16) | (final_a&0xffff);
    *w++ = (DrDx_fixed<<16) | (DgDx_fixed&0xffff);
    *w++ = (DbDx_fixed<<16) | (DaDx_fixed&0xffff);
    *w++ = (DrDe_fixed&0xffff0000) | (0xffff&(DgDe_fixed>>16));
    *w++ = (DbDe_fixed&0xffff0000) | (0xffff&(DaDe_fixed>>16));
    *w++ = (DrDy_fixed&0xffff0000) | (0xffff&(DgDy_fixed>>16));
    *w++ = (DbDy_fixed&0xffff0000) | (0xffff&(DaDy_fixed>>16));
    *w++ = (DrDe_fixed<<16) | (DgDe_fixed&0xffff);
    *w++ = (DbDe_fixed<<16) | (DaDe_fixed&0xffff);
    *w++ = (DrDy_fixed<<16) | (DgDy_fixed&0xffff);
    *w++ = (DbDy_fixed<<16) | (DaDy_fixed&0xffff);

    tracef("b1: %f (%08lx)\n", v1[2], (uint32_t)(v1[2]*255.0f));
    tracef("b2: %f (%08lx)\n", v2[2], (uint32_t)(v2[2]*255.0f));
    tracef("b3: %f (%08lx)\n", v3[2], (uint32_t)(v3[2]*255.0f));
    tracef("mb: %f (%08lx)\n", mb, (uint32_t)(int32_t)mb);
    tracef("hb: %f (%08lx)\n", hb, (uint32_t)(int32_t)hb);
    tracef("nxB: %f (%08lx)\n", nxB, (int32_t)(nxB * 4.0f));
    tracef("DbDx: %f (%08lx)\n", DbDx, (uint32_t)(DbDx * 65536.0f));
    tracef("DbDx_fixed: (%08lx)\n", DbDx_fixed);

    return w;
}

#if 0
__attribute__((always_inline))
static inline void __rdpq_write_tex_coeffs(rspq_write_t *w, rdpq_tri_edge_data_t *data, const float *v1, const float *v2, const float *v3)
{
    float s1 = v1[0] * 32.f, t1 = v1[1] * 32.f, invw1 = v1[2];
    float s2 = v2[0] * 32.f, t2 = v2[1] * 32.f, invw2 = v2[2];
    float s3 = v3[0] * 32.f, t3 = v3[1] * 32.f, invw3 = v3[2];

    const float minw = 1.0f / MAX(MAX(invw1, invw2), invw3);

    tracef("s1: %f (%04x)\n", s1, (int16_t)s1);
    tracef("t1: %f (%04x)\n", t1, (int16_t)t1);
    tracef("s2: %f (%04x)\n", s2, (int16_t)s2);
    tracef("t2: %f (%04x)\n", t2, (int16_t)t2);

    tracef("invw1: %f (%08lx)\n", invw1, (int32_t)(invw1*65536));
    tracef("invw2: %f (%08lx)\n", invw2, (int32_t)(invw2*65536));
    tracef("invw3: %f (%08lx)\n", invw3, (int32_t)(invw3*65536));
    tracef("minw: %f (%08lx)\n", minw, (int32_t)(minw*65536));

    invw1 *= minw;
    invw2 *= minw;
    invw3 *= minw;

    s1 *= invw1;
    t1 *= invw1;
    s2 *= invw2;
    t2 *= invw2;
    s3 *= invw3;
    t3 *= invw3;

    invw1 *= 0x7FFF;
    invw2 *= 0x7FFF;
    invw3 *= 0x7FFF;

    const float ms = s2 - s1;
    const float mt = t2 - t1;
    const float mw = invw2 - invw1;
    const float hs = s3 - s1;
    const float ht = t3 - t1;
    const float hw = invw3 - invw1;

    const float nxS = data->hy*ms - data->my*hs;
    const float nxT = data->hy*mt - data->my*ht;
    const float nxW = data->hy*mw - data->my*hw;
    const float nyS = data->mx*hs - data->hx*ms;
    const float nyT = data->mx*ht - data->hx*mt;
    const float nyW = data->mx*hw - data->hx*mw;

    const float DsDx = nxS * data->attr_factor;
    const float DtDx = nxT * data->attr_factor;
    const float DwDx = nxW * data->attr_factor;
    const float DsDy = nyS * data->attr_factor;
    const float DtDy = nyT * data->attr_factor;
    const float DwDy = nyW * data->attr_factor;

    const float DsDe = DsDy + DsDx * data->ish;
    const float DtDe = DtDy + DtDx * data->ish;
    const float DwDe = DwDy + DwDx * data->ish;

    const int32_t final_s = float_to_s16_16(s1 + data->fy * DsDe);
    const int32_t final_t = float_to_s16_16(t1 + data->fy * DtDe);
    const int32_t final_w = float_to_s16_16(invw1 + data->fy * DwDe);

    const int32_t DsDx_fixed = float_to_s16_16(DsDx);
    const int32_t DtDx_fixed = float_to_s16_16(DtDx);
    const int32_t DwDx_fixed = float_to_s16_16(DwDx);

    const int32_t DsDe_fixed = float_to_s16_16(DsDe);
    const int32_t DtDe_fixed = float_to_s16_16(DtDe);
    const int32_t DwDe_fixed = float_to_s16_16(DwDe);

    const int32_t DsDy_fixed = float_to_s16_16(DsDy);
    const int32_t DtDy_fixed = float_to_s16_16(DtDy);
    const int32_t DwDy_fixed = float_to_s16_16(DwDy);

    rspq_write_arg(w, (final_s&0xffff0000) | (0xffff&(final_t>>16)));  
    rspq_write_arg(w, (final_w&0xffff0000));
    rspq_write_arg(w, (DsDx_fixed&0xffff0000) | (0xffff&(DtDx_fixed>>16)));
    rspq_write_arg(w, (DwDx_fixed&0xffff0000));    
    rspq_write_arg(w, (final_s<<16) | (final_t&0xffff));
    rspq_write_arg(w, (final_w<<16));
    rspq_write_arg(w, (DsDx_fixed<<16) | (DtDx_fixed&0xffff));
    rspq_write_arg(w, (DwDx_fixed<<16));
    rspq_write_arg(w, (DsDe_fixed&0xffff0000) | (0xffff&(DtDe_fixed>>16)));
    rspq_write_arg(w, (DwDe_fixed&0xffff0000));
    rspq_write_arg(w, (DsDy_fixed&0xffff0000) | (0xffff&(DtDy_fixed>>16)));
    rspq_write_arg(w, (DwDy_fixed&0xffff0000));
    rspq_write_arg(w, (DsDe_fixed<<16) | (DtDe_fixed&0xffff));
    rspq_write_arg(w, (DwDe_fixed<<16));
    rspq_write_arg(w, (DsDy_fixed<<16) | (DtDy_fixed&&0xffff));
    rspq_write_arg(w, (DwDy_fixed<<16));

    tracef("invw1-mul: %f (%08lx)\n", invw1, (int32_t)(invw1*65536));
    tracef("invw2-mul: %f (%08lx)\n", invw2, (int32_t)(invw2*65536));
    tracef("invw3-mul: %f (%08lx)\n", invw3, (int32_t)(invw3*65536));

    tracef("s1w: %f (%04x)\n", s1, (int16_t)s1);
    tracef("t1w: %f (%04x)\n", t1, (int16_t)t1);
    tracef("s2w: %f (%04x)\n", s2, (int16_t)s2);
    tracef("t2w: %f (%04x)\n", t2, (int16_t)t2);

    tracef("ms: %f (%04x)\n", ms, (int16_t)(ms));
    tracef("mt: %f (%04x)\n", mt, (int16_t)(mt));
    tracef("hs: %f (%04x)\n", hs, (int16_t)(hs));
    tracef("ht: %f (%04x)\n", ht, (int16_t)(ht));

    tracef("nxS: %f (%04x)\n", nxS, (int16_t)(nxS / 65536.0f));
    tracef("nxT: %f (%04x)\n", nxT, (int16_t)(nxT / 65536.0f));
    tracef("nyS: %f (%04x)\n", nyS, (int16_t)(nyS / 65536.0f));
    tracef("nyT: %f (%04x)\n", nyT, (int16_t)(nyT / 65536.0f));
}
#endif

__attribute__((always_inline))
static inline uint32_t* __rdpq_write_zbuf_coeffs(uint32_t *w, rdpq_tri_edge_data_t *data, const float *v1, const float *v2, const float *v3)
{
    const float z1 = v1[0] * 0x7FFF;
    const float z2 = v2[0] * 0x7FFF;
    const float z3 = v3[0] * 0x7FFF;

    const float mz = z2 - z1;
    const float hz = z3 - z1;

    const float nxz = data->hy*mz - data->my*hz;
    const float nyz = data->mx*hz - data->hx*mz;

    const float DzDx = nxz * data->attr_factor;
    const float DzDy = nyz * data->attr_factor;
    const float DzDe = DzDy + DzDx * data->ish;

    const int32_t final_z = float_to_s16_16(z1 + data->fy * DzDe);
    const int32_t DzDx_fixed = float_to_s16_16(DzDx);
    const int32_t DzDe_fixed = float_to_s16_16(DzDe);
    const int32_t DzDy_fixed = float_to_s16_16(DzDy);

    *w++ = final_z;
    *w++ = DzDx_fixed;
    *w++ = DzDe_fixed;
    *w++ = DzDy_fixed;

    tracef("z1: %f (%04x)\n", v1[0], (uint16_t)(z1));
    tracef("z2: %f (%04x)\n", v2[0], (uint16_t)(z2));
    tracef("z3: %f (%04x)\n", v3[0], (uint16_t)(z3));

    tracef("mz: %f (%04x)\n", mz, (uint16_t)(mz));
    tracef("hz: %f (%04x)\n", hz, (uint16_t)(hz));

    tracef("nxz: %f (%08lx)\n", nxz, (uint32_t)(nxz * 4.0f));
    tracef("nyz: %f (%08lx)\n", nyz, (uint32_t)(nyz * 4.0f));

    tracef("dzdx: %f (%08llx)\n", DzDx, (uint64_t)(DzDx * 65536.0f));
    tracef("dzdy: %f (%08llx)\n", DzDy, (uint64_t)(DzDy * 65536.0f));
    tracef("dzde: %f (%08llx)\n", DzDe, (uint64_t)(DzDe * 65536.0f));

    return w;
}

uint32_t* rdpq_triangle_cpu(uint32_t *w, const float *v1, const float *v2, const float *v3)
{
    float cull_side = 1.0f;
    uint32_t *worig = w;
    if( v1[VTX_POS_OFFSET + 1] > v2[VTX_POS_OFFSET + 1] ) { SWAP(v1, v2); cull_side = -cull_side; }
    if( v2[VTX_POS_OFFSET + 1] > v3[VTX_POS_OFFSET + 1] ) { SWAP(v2, v3); cull_side = -cull_side; }
    if( v1[VTX_POS_OFFSET + 1] > v2[VTX_POS_OFFSET + 1] ) { SWAP(v1, v2); cull_side = -cull_side; }

    rdpq_tri_edge_data_t data;
    w = __rdpq_write_edge_coeffs(w, &data, 0, 0, cull_side, v1 + VTX_POS_OFFSET, v2 + VTX_POS_OFFSET, v3 + VTX_POS_OFFSET);
    if (!w) return worig;

    #if 0
    if (fmt->shade_offset >= 0) {
        const float *shade_v2 = fmt->shade_flat ? v1 : v2;
        const float *shade_v3 = fmt->shade_flat ? v1 : v3;
        __rdpq_write_shade_coeffs(&w, &data, v1 + fmt->shade_offset, shade_v2 + fmt->shade_offset, shade_v3 + fmt->shade_offset);
    }

    if (fmt->tex_offset >= 0) {
        __rdpq_write_tex_coeffs(&w, &data, v1 + fmt->tex_offset, v2 + fmt->tex_offset, v3 + fmt->tex_offset);
    }
    #endif

    if (VTX_Z_OFFSET >= 0) {
        w = __rdpq_write_zbuf_coeffs(w, &data, v1 + VTX_Z_OFFSET, v2 + VTX_Z_OFFSET, v3 + VTX_Z_OFFSET);
    }
    
    return w;
}

static float xangle = MM_PI/8;
static float yangle = MM_PI/4; 

static uint32_t* tnl(float *vtx, int nvtx, uint32_t *w)
{
    const float viewDist = 2.5f;
    const int viewWidth = 320;
    const int viewHeight = 240;
    const float aspect = (float)viewWidth / (float)viewHeight;
    const float focal = 1.0f;

    float screen[4][3];

    for (int i=0; i<nvtx; i+=4) {
        for (int j=0; j<4; j++) {
            float x = vtx[0];
            float y = vtx[1];
            float z = vtx[2];
            vtx += 3;

            // Rotate around the Y axis
            float x0 = x * mm_cosf(yangle) - y * mm_sinf(yangle);
            float y0 = x * mm_sinf(yangle) + y * mm_cosf(yangle);
            x = x0;
            y = y0;

            // Rotate around the X axis
            float y1 = y * mm_cosf(xangle) - z * mm_sinf(xangle);
            float z1 = y * mm_sinf(xangle) + z * mm_cosf(xangle);
            y = y1;
            z = z1;

            // Project
            z += viewDist;
            x = x * focal / z;
            y = y * focal * aspect / z;

            screen[j][0] = (x + 1.0f) * viewWidth / 2.0f;
            screen[j][1] = (y + 1.0f) * viewHeight / 2.0f;
            screen[j][2] = z / viewDist;
        }

        w = rdpq_triangle_cpu(w, screen[0], screen[1], screen[2]);
        w = rdpq_triangle_cpu(w, screen[0], screen[2], screen[3]);
    }
    return w;
}

void getCirclePoint(float t, float *outX, float *outY) {
    float x, y;
    if (t < 0.25f) {
        // Top edge: from (1, 1) to (1, -1)
        x = 1.0f;
        y = 1.0f - 8.0f * t; // When t=0 => y=1, when t=0.25 => y=-1.
    } else if (t < 0.5f) {
        // Right edge: from (1, -1) to (-1, -1)
        float u = t - 0.25f;
        x = 1.0f - 8.0f * u; // When t=0.25 => x=1, when t=0.5 => x=-1.
        y = -1.0f;
    } else if (t < 0.75f) {
        // Bottom edge: from (-1, -1) to (-1, 1)
        float u = t - 0.5f;
        x = -1.0f;
        y = -1.0f + 8.0f * u; // When t=0.5 => y=-1, when t=0.75 => y=1.
    } else {
        // Left edge: from (-1, 1) to (1, 1)
        float u = t - 0.75f;
        x = -1.0f + 8.0f * u; // When t=0.75 => x=-1, when t=1 => x=1.
        y = 1.0f;
    }
    // Normalize (x,y) to produce a point on the unit circle.
    float len = __builtin_sqrtf(x * x + y * y);
    *outX = x / len;
    *outY = y / len;
}


void mesh(void)
{
    xangle += 0.01f;
    yangle += 0.01f;

    // Number of segments for the major (u) and minor (v) circles.
    int numMajor = 24; // Segments along the torus' main circle.
    int numMinor = 16; // Segments along the tube (minor circle).

    // Torus parameters: major radius (R) and minor radius (r).
    float R = 1.0f;  // Distance from the center of the torus to the center of the tube.
    float r = 0.3f;  // Radius of the tube.

    int numQuads = numMajor * numMinor;
    int totalVertices = numQuads * 4;

    u3d_vertex *vtx = VERTEX_BUFFER;
    uint64_t *w64 = RDP_BUFFER;

    *w64++ = RdpSyncPipe();
    *w64++ = RdpSetOtherModes(SOM_CYCLE_1 | SOM_Z_COMPARE | SOM_Z_WRITE);
    *w64++ = RdpSetCombine(RDPQ_COMBINER_FLAT);
    *w64++ = RdpSetPrimColor(RGBA32(0xFF, 0xFF, 0xFF, 0xFF));
    uint32_t *w = (uint32_t*)w64;

#if 1
    static bool created = false;
    if (!created) {
        // Loop over the torus patches.
        for (int i = 0; i < numMajor; i++) {
            // Calculate the current and next angle for the major circle.
            float u0 = 2.0f * MM_PI * i / numMajor;
            float u1 = 2.0f * MM_PI * ((i + 1) % numMajor) / numMajor;
            
            for (int j = 0; j < numMinor; j++) {
                // Calculate the current and next angle for the minor circle.
                float v0 = 2.0f * MM_PI * j / numMinor;
                float v1 = 2.0f * MM_PI * ((j + 1) % numMinor) / numMinor;

                // Compute the four vertices for the current quad.
                float rcv0 = R + r * mm_cosf(v0);
                float rcv1 = R + r * mm_cosf(v1);

                #define TO_U16(x) ((int16_t)(x * 64.0f))

                vtx[0].pos[0] = TO_U16(rcv0 * mm_cosf(u0));
                vtx[0].pos[1] = TO_U16(rcv0 * mm_sinf(u0));
                vtx[0].pos[2] = TO_U16(r * mm_sinf(v0));
              
                vtx[1].pos[0] = TO_U16(rcv0 * mm_cosf(u1));
                vtx[1].pos[1] = TO_U16(rcv0 * mm_sinf(u1));
                vtx[1].pos[2] = TO_U16(r * mm_sinf(v0));

                vtx[2].pos[0] = TO_U16(rcv1 * mm_cosf(u1));
                vtx[2].pos[1] = TO_U16(rcv1 * mm_sinf(u1));
                vtx[2].pos[2] = TO_U16(r * mm_sinf(v1));

                vtx[3].pos[0] = TO_U16(rcv1 * mm_cosf(u0));
                vtx[3].pos[1] = TO_U16(rcv1 * mm_sinf(u0));
                vtx[3].pos[2] = TO_U16(r * mm_sinf(v1));

                vtx += 4;
            }
        }

        vtx[0].pos[0] = 0xFFFF;
        created = true;
    }

    //w = tnl((float*)VERTEX_BUFFER, totalVertices, w);


#else
    // Loop over major (u0, u1) and minor (v0, v1) parameters.
    for (int i = 0; i < numMajor; i++) {
        float u0 = (float)i / numMajor;          // parameter for current major segment
        float u1 = (float)(i + 1) / numMajor;      // parameter for next major segment
        float majorX0, majorY0, majorX1, majorY1;
        getCirclePoint(u0, &majorX0, &majorY0);
        getCirclePoint(u1, &majorX1, &majorY1);

        for (int j = 0; j < numMinor; j++) {
            float v0 = (float)j / numMinor;      // parameter for current minor segment
            float v1 = (float)(j + 1) / numMinor;  // parameter for next minor segment
            float minorX0, minorY0, minorX1, minorY1;
            getCirclePoint(v0, &minorX0, &minorY0);
            getCirclePoint(v1, &minorX1, &minorY1);

            // Compute the four vertices of the current quad.
            // Vertex v0: (u0, v0)
            *vtx++ = (R + r * minorX0) * majorX0;
            *vtx++ = (R + r * minorX0) * majorY0;
            *vtx++ = r * minorY0;

            // Vertex v1: (u1, v0)
            *vtx++ = (R + r * minorX0) * majorX1;
            *vtx++ = (R + r * minorX0) * majorY1;
            *vtx++ = r * minorY0;

            // Vertex v2: (u1, v1)
            *vtx++ = (R + r * minorX1) * majorX1;
            *vtx++ = (R + r * minorX1) * majorY1;
            *vtx++ = r * minorY1;

            // Vertex v3: (u0, v1)
            *vtx++ = (R + r * minorX1) * majorX0;
            *vtx++ = (R + r * minorX1) * majorY0;
            *vtx++ = r * minorY1;

            w = tnl(vtx-12, 4, w);
            // w64 = (uint64_t*)w;
            // *w64++ = RdpSetPrimColor(RGBA32(0xFF, 0x40+j*0x10, 0xE0-j*0x10, 0xFF));
            // w = (uint32_t*)w64;
            // w = rdpq_triangle_cpu(w, vtx-12, vtx-9, vtx-6);
            // w = rdpq_triangle_cpu(w, vtx-12, vtx-6, vtx-3);
        }
    }

#endif
    /*w64 = (uint64_t*)w;
    *w64++ = RdpSyncFull();

    debugf("Vertices: %p-%p (%d)\n", VERTEX_BUFFER, vtx, totalVertices);
    debugf("Sending %p-%p\n", RDP_BUFFER, w64);
    // Draw
    *DP_START = (uint32_t)RDP_BUFFER;
    *DP_END = (uint32_t)w64;
    while (*DP_STATUS & DP_STATUS_PIPE_BUSY) {};
    */
}
