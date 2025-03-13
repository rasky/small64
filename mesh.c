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

uint32_t mesh(void)
{
    xangle += 0.01f;
    yangle += 0.01f;

    // Number of segments for the major (u) and minor (v) circles.
    int numMajor = 24; // Segments along the torus' main circle.
    int numMinor = 16; // Segments along the tube (minor circle).

    // Torus parameters: major radius (R) and minor radius (r).
    float R = 1.0f * 64;  // Distance from the center of the torus to the center of the tube.
    float r = 0.3f * 64;  // Radius of the tube.

    int numQuads = numMajor * numMinor;
    int totalVertices = numQuads * 4;

#if 1
    static u3d_vertex *vtx = VERTEX_BUFFER;
    if (vtx == VERTEX_BUFFER) {
        
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

                vtx[0].pos[0] = (int8_t)(rcv0 * mm_cosf(u0));
                vtx[0].pos[1] = (int8_t)(rcv0 * mm_sinf(u0));
                vtx[0].pos[2] = (int8_t)(r * mm_sinf(v0));
                            
                vtx[1].pos[0] = (int8_t)(rcv0 * mm_cosf(u1));
                vtx[1].pos[1] = (int8_t)(rcv0 * mm_sinf(u1));
                vtx[1].pos[2] = (int8_t)(r * mm_sinf(v0));
 
                vtx[2].pos[0] = (int8_t)(rcv1 * mm_cosf(u1));
                vtx[2].pos[1] = (int8_t)(rcv1 * mm_sinf(u1));
                vtx[2].pos[2] = (int8_t)(r * mm_sinf(v1));

                vtx[3].pos[0] = (int8_t)(rcv1 * mm_cosf(u0));
                vtx[3].pos[1] = (int8_t)(rcv1 * mm_sinf(u0));
                vtx[3].pos[2] = (int8_t)(r * mm_sinf(v1));
                
                vtx[0].normal[0] = (mm_cosf(u0) * mm_cosf(v0)) * 127;
                vtx[0].normal[1] = (mm_sinf(u0) * mm_cosf(v0)) * 127;
                vtx[0].normal[2] = (mm_sinf(v0)) * 127;

                vtx[1].normal[0] = (mm_cosf(u1) * mm_cosf(v0)) * 127;
                vtx[1].normal[1] = (mm_sinf(u1) * mm_cosf(v0)) * 127;
                vtx[1].normal[2] = (mm_sinf(v0)) * 127;

                vtx[2].normal[0] = (mm_cosf(u1) * mm_cosf(v1)) * 127;
                vtx[2].normal[1] = (mm_sinf(u1) * mm_cosf(v1)) * 127;
                vtx[2].normal[2] = (mm_sinf(v1)) * 127;
                
                vtx[3].normal[0] = (mm_cosf(u0) * mm_cosf(v1)) * 127;
                vtx[3].normal[1] = (mm_sinf(u0) * mm_cosf(v1)) * 127; 
                vtx[3].normal[2] = (mm_sinf(v1)) * 127;

                vtx[4] = vtx[0];
                vtx[5] = vtx[2];
                vtx += 6;
            }
        }
    }

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
   return (uint32_t)vtx;
}
