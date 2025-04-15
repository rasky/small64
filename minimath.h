#ifndef MINIMATH_H
#define MINIMATH_H

__attribute__((noinline))
static float random(void)
{
    // static uint32_t seed = 0x123456;
    // seed = seed * 1664525 + 1013904223;
    // return (float)(seed & 0x7FFFFFFF) / 0x7FFFFFFF;
    return (C0_COUNT() & 0xFFF) * (1.0f / 4095.0f);
}

__attribute__((noinline))
static uint32_t random_u32(void)
{
    static uint32_t state = 1;
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

static inline uint8_t random_u8(void)
{
    return random_u32() & 0xFF;
}

static const float MM_PI            = 3.14159274e+00f;

__attribute__((const))
static inline float mm_floorf(float x) {
    float y, yint;
    __asm ("floor.w.s  %0,%1" : "=f"(yint) : "f"(x));
    __asm ("cvt.s.w  %0,%1" : "=f"(y) : "f"(yint));
    return y;
}

__attribute__((const))
static inline float mm_truncf(float x) {
    /* Notice that trunc.w.s is also emitted by the compiler when casting a
     * float to int, but in this case we want a floating point result anyway,
     * so it's useless to go back and forth a GPR. */
    float yint, y;
    __asm ("trunc.w.s  %0,%1" : "=f"(yint) : "f"(x));
    __asm ("cvt.s.w  %0,%1" : "=f"(y) : "f"(yint));
    return y;
}

// __attribute__((const))
// static inline float mm_fmodf(float x, float y) {
//     return x - mm_truncf(x * (1.0f / y)) * y;
// }

__attribute__((const))
static float mm_fmodf(float x, float y)
{
    float q = mm_truncf(x / y);
    return x - q * y;
}

__attribute__((const))
static float mm_remf(float x, int y)
{
    int ix = (int)x;
    return x - ix + (ix % y);
}

#if 0
// x must be in [-π, +π] range
static float mm_sinf0(float x) {
    if (x < 0)
        return 1.27323954f * x + 0.405284735f * x * x;
    else
        return 1.27323954f * x - 0.405284735f * x * x;
}

static float mm_cosf0(float x) {
    return mm_sinf0(x + 1.57079632679489661923f);
}
#endif

__attribute__((const))
static float mm_sinf(float x) {
    while (x < -MM_PI) x += 2 * MM_PI;
    while (x >  MM_PI) x -= 2 * MM_PI;

    const float B = 4.0f / MM_PI;
    const float C = -4.0f / (MM_PI * MM_PI);
    float y = B * x + C * x * __builtin_fabsf(x);

    const float P = 0.225f;
    return P * (y * __builtin_fabsf(y) - y) + y;
}

__attribute__((const))
static float mm_cosf(float x) {
    return mm_sinf(x + MM_PI/2);
}

#define mm_cosf(x)   (__builtin_constant_p(x) ? __builtin_cosf(x) : mm_cosf(x))
#define mm_sinf(x)   (__builtin_constant_p(x) ? __builtin_sinf(x) : mm_sinf(x))

__attribute__((const))
int mm_sin_s8(int s) {
  // Note: using a lookup table created during runtime is more code
  // even though we would avoid an index to float mapping here
  return mm_sinf(s * (MM_PI / 128.0f)) * 0x7F;
}

__attribute__((const))
static int mm_cos_s8(int s) {
  return mm_sin_s8(s + 64);
}

#define LN2             0.693147f
#define LN3             1.098612f
#define LN03            0.105360f

__attribute__((const))
static float pow_approx(float ln, float n) {
    float x = n * ln;
    float result = 1.0f + x + (x * x) / 2.0f + (x * x * x) / 6.0f;
    return result;
}

#endif
