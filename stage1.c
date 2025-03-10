#include <stdint.h>
#include <stdbool.h>
#include "minidragon.h"
#include "debug.h"
#include "rdram.h"
#include "loader.h"
#include "stage1.h"

// These register contains boot flags passed by IPL2. Define them globally
// during the first stage of IPL3, so that the registers are not reused.
register uint32_t ipl2_romType   asm ("s3");
register uint32_t ipl2_tvType    asm ("s4");
register uint32_t ipl2_resetType asm ("s5");
register uint32_t ipl2_romSeed   asm ("s6");
register uint32_t ipl2_version   asm ("s7");

__attribute__((section(".stage1")))
int stage1(void)
{    
    rdram_init();

    *IPL3_TV_TYPE = ipl2_tvType;
    *IPL3_IQUE = (*MI_VERSION & 0xF0) == 0xB0;

    // Clear COP0 cache. At boot, it may contain stale data that might cause
    // invalid cache writes.
    cop0_clear_cache();

    // Acknowledge the boot with PIF
    si_write(0x7FC, 0x8);

    const int memsize = 4*1024*1024;
    return memsize;
}
