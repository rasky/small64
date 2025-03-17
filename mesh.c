#include "minimath.h"
#include <stdint.h>
#include "rdp_commands.h"

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
    xangle += 0.01f;
    yangle += 0.01f;

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
