#include "minilib.h"

void memset32(void *dst, uint32_t c, unsigned n)
{
    uint32_t *dst32 = dst;
    uint32_t *end = dst+n;
    while (dst32 < end)
        *dst32++ = c;
}

