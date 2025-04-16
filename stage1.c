/**************************************************************************
 * Minimal N64 RDRAM initialization code
 * Written by Giovanni Bajo (giovannibajo@gmail.com)
 **************************************************************************
 * This is loosely based on rdram.c as found in libdragon's IPL3 but it is
 * heavily stripped down to the bare minimum, by cutting lots of shortcuts.
 * 
 * In particular, all business logic is basically removed leaving only a
 * series of register writes that are necessary to initialize the RDRAM.
 * 
 * These are the main changes in the initialization algorithm:
 * 
 * * No current calibration is performed. The current is set to the "magic
 *   value" of 0x30 that appears to work on all consoles we tested.
 * * Automatic current tuning is not enabled. RAMBUS suggests to enable that
 *   do deal with temperature changes, but 4K intros will run for a few minutes
 *   at most, so we can hopefully skip that (and save a lot of code).
 * * The RAS interval is set to a fixed value that is expected to work on all
 *   RDRAM chips shipped with the N64. Otherwise, querying the chip model would
 *   be necessary.
 * * RDRAM refresh rate is not configured. The default appears to work correctly
 *   and doesn't seem to impact memory bandwidth that much, as the n64brew wiki
 *   implies
 * * The code is configured at compile-time for either 4, 6 or 8 MiB of RDRAM,
 *   but does not handle runtime errors. So if configure for 8 Mib, the expansion
 *   pak must be present, otherwise the code will hang. 
 * 
 **************************************************************************/

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

// Define how many banks must be initialized. Each bank is 2 MiB of RAM.
// So 2 banks means 4 MiB, or 4 banks means 8 MiB.
// Notice that there is no error detection so if you ask for 8 MiB, the
// code will hang if expansion pak is not present.
#define NUM_RDRAM_BANKS           2

// Configure the RDRAM refresh rate. This is described as required in the
// n64brew wiki, but it seems like the defaults are reasonable and don't cause
// much of a performance hit.
#define REFRESH_RATE              0

typedef struct {
    volatile uint32_t *reg;
    uint32_t value;
} reg_write_t; 

// Inizization of a single chip given its ID
#define CHIP_INIT(chip_id) \
    { &RDRAM_REGS(0x7F)[RDRAM_REG_DEVICE_ID],         RDRAM_REG_DEVICE_ID_MAKE(chip_id) }, \
    { &RDRAM_REGS(chip_id)[RDRAM_REG_MODE],           RDRAM_REG_MODE_DE | RDRAM_REG_MODE_AS | 0x40000000 | RDRAM_REG_MODE_CC(0x30) }, \
    { &RDRAM_REGS(chip_id)[RDRAM_REG_RAS_INTERVAL],   RDRAM_REG_RASINTERVAL_MAKE(1, 7, 10, 4) }

// RDRAM initialization registers
const reg_write_t rdram_init_regs[] = {
    { RI_CONFIG,
            RI_CONFIG_AUTO_CALIBRATION },
    { RI_CURRENT_LOAD,
            0 },
    { RI_SELECT,
            RI_SELECT_RX_TX, },
    { RI_REFRESH,
            0 },
    { RI_MODE,
            RI_MODE_STANDARD },
    { MI_MODE,
            MI_WMODE_SET_REPEAT_MODE | MI_WMODE_REPEAT_LENGTH(16) },
    { &RDRAM_REGS_BROADCAST[RDRAM_REG_DELAY],
            ROT16(RDRAM_REG_DELAY_MAKE(5, 7, 3, 1)) }, 
    { &RDRAM_REGS_BROADCAST[RDRAM_REG_DEVICE_ID],
            RDRAM_REG_DEVICE_ID_MAKE(0x7F) },
    { &RDRAM_REGS_BROADCAST[RDRAM_REG_REF_ROW],
            0 },

    // Initialize all the RDRAM chips (2 MiB each one)
    CHIP_INIT(0),
#if NUM_RDRAM_BANKS > 1
    CHIP_INIT(2),
#endif
#if NUM_RDRAM_BANKS > 2
    CHIP_INIT(4),
#endif
#if NUM_RDRAM_BANKS > 3
    CHIP_INIT(6),
#endif
#if REFRESH_RATE
    { RI_REFRESH,
        RI_REFRESH_AUTO | RI_REFRESH_OPTIMIZE |  RI_REFRESH_CLEANDELAY(52)
        | RI_REFRESH_DIRTYDELAY(54)
        | RI_REFRESH_MULTIBANK((1 << NUM_RDRAM_BANKS) - 1) },
#endif
    { 0, 0 }, // terminator
};

// Wait for a number of cycles. This compresses very well, up to the point that
// it's better than even an inlined 3-opcode loop.
#define WAIT8()   asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop"); asm("nop")
#define WAIT32()  WAIT8(); WAIT8(); WAIT8(); WAIT8()
#define WAIT128() WAIT32(); WAIT32(); WAIT32(); WAIT32()

__attribute__((section(".stage1")))
int stage1(void)
{    
#if 0
    extern int mini_rdram_init(void);
    int memsize = mini_rdram_init();
#else
    const reg_write_t *init = rdram_init_regs;
    while (1) {
        if (!init->reg) break;
        *init->reg = init->value;
        init++;
        // Some delay is required after some of the writes of the initialization
        // routine. We always delay as it doesn't matter, and do that with
        // a series of nops that compress very well.
        WAIT128();
    }
    const int memsize = NUM_RDRAM_BANKS * 2 * 1024 * 1024;
#endif

    #if 0
    // If needed, you can save the TV type in the RDRAM for later use.
    *IPL3_TV_TYPE = ipl2_tvType;
    #endif

    // Clear COP0 cache. At boot, it may contain stale data that might cause
    // invalid cache writes.
    cop0_clear_cache();

    // Acknowledge the boot with PIF
    si_write(0x7FC, 0x8);

    // Prepare GP for stage2
    extern char _gp;
    asm("la $gp, %0"::"i"(&_gp));

    return memsize+0x80000000; // return the initial stack pointer position
}
