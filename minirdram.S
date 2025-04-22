###########################################################################
# Minimal N64 RDRAM initialization code
# Written by Giovanni Bajo (giovannibajo@gmail.com)
###########################################################################
#
# Full RDRAM initialization code can be seen in libdragon's IPL3:
# https://github.com/DragonMinded/libdragon/tree/trunk/boot/rdram.c
#
# This version here does not follow the current calibration process described
# by Rambus datasheets, but configures chips with a fixed output current
# (setting the RDRAM mode CC bits to 0x30). This seems to work reasonably
# fine for slightly warmed up consoles, while it sometimes fails for cold units.
# 
# Also, we don't configure the automatic current finetuning; that is meant
# to make the current output by the chips follow temperature changes, but for
# a 2 minutes intro, this is not required.
#
# Doing these simplifications means that initialization is now just a 
# sequence of register writes with fixed values. We just prepare a table with
# such values and execute them.
###########################################################################


# Define how many banks must be initialized. Each bank is 2 MiB of RAM.
# So 2 banks means 4 MiB, or 4 banks means 8 MiB.
# Notice that there is no error detection so if you ask for 8 MiB, the
# code will hang if expansion pak is not present.
#define NUM_RDRAM_BANKS           2


    .set noreorder
    .set at

    .section .text.stage1, "ax"
    .globl stage1

#define inbuf           $s1
#define outbuf          $s2
#define outbuf_end      $s0
#define decomp_return   $a3
#define dmem_base       $t3         // Base address for lax macro (set by IPL2)


#define C0_TAGLO                $28
#define C0_TAGHI                $29
#define INDEX_STORE_TAG_D       0x9
#define INDEX_STORE_TAG_I       0x8

#define BIT(x, n)               (((x) >> (n)) & 1)
#define BITS(x, b, e)           (((x) >> (b)) & ((1 << ((e)-(b)+1))-1))
#define BITSWAP5(x)             ((BIT(x,0)<<4) | (BIT(x,1)<<3) | (BIT(x,2)<<2) | (BIT(x,3)<<1) | (BIT(x,4)<<0))
#define ROT16(x)                ((((x) & 0xFFFF0000) >> 16) | (((x) & 0xFFFF) << 16))

#define MI_BASE                 0xA4300000
#define RI_BASE                 0xA4700000
#define RDRAM_REGS(chip)        (0xA3F00000 + ((chip) << 10))
#define RDRAM_REGS_BROADCAST    0xA3F80000

#define RI_MODE                 (RI_BASE + 0x00)
#define RI_CONFIG               (RI_BASE + 0x04)
#define RI_CURRENT_LOAD         (RI_BASE + 0x08)
#define RI_SELECT               (RI_BASE + 0x0C)
#define RI_REFRESH              (RI_BASE + 0x10)

#define MI_MODE                 (MI_BASE + 0x00)

#define RI_CONFIG_AUTO_CALIBRATION   0x40
#define RI_SELECT_RX_TX              0x14
#define RI_MODE_RESET                0x0
#define RI_MODE_STANDARD             (0x8|0x4|0x2)

#define RI_REFRESH_CLEANDELAY(x)            ((x) & 0xFF)
#define RI_REFRESH_DIRTYDELAY(x)            (((x) & 0xFF) << 8)
#define RI_REFRESH_AUTO                     (1<<17)
#define RI_REFRESH_OPTIMIZE                 (1<<18)
#define RI_REFRESH_MULTIBANK(x)             (((x) & 0xF) << 19)

#define MI_WMODE_CLEAR_REPEAT_MOD           0x80
#define MI_WMODE_SET_REPEAT_MODE            0x100
#define MI_WMODE_REPEAT_LENGTH(n)           ((n)-1)
#define MI_WMODE_SET_UPPER_MODE             0x2000
#define MI_WMODE_CLEAR_UPPER_MODE           0x1000

#define PIF_COMMAND                         0xBFC007FC
#define PIF_COMMAND_ACK                     0x8

#define RDRAM_REG_DEVICE_ID          (1 * 4)
#define RDRAM_REG_DELAY              (2 * 4)
#define RDRAM_REG_MODE               (3 * 4)
#define RDRAM_REG_REF_ROW            (5 * 4)
#define RDRAM_REG_RAS_INTERVAL       (6 * 4)

#define RDRAM_REG_MODE_DE       (1<<25)
#define RDRAM_REG_MODE_AS       (1<<26)
#define RDRAM_REG_MODE_CC(cc)   ((BIT((cc)^0x3F, 0) << 6)  | \
                                (BIT((cc)^0x3F, 1) << 14) | \
                                (BIT((cc)^0x3F, 2) << 22) | \
                                (BIT((cc)^0x3F, 3) << 7)  | \
                                (BIT((cc)^0x3F, 4) << 15) | \
                                (BIT((cc)^0x3F, 5) << 23))

#define RDRAM_REG_DELAY_MAKE(AckWinDelay, ReadDelay, AckDelay, WriteDelay) \
   ((((AckWinDelay) & 7) << 3 << 24) | \
    (((ReadDelay)   & 7) << 3 << 16) | \
    (((AckDelay)    & 3) << 3 <<  8) | \
    (((WriteDelay)  & 7) << 3 <<  0))

#define RDRAM_REG_DEVICE_ID_MAKE(chip_id) \
    (BITS(chip_id, 0, 5) << 26 | \
     BITS(chip_id, 6, 6) << 23 | \
     BITS(chip_id, 7, 14) << 8 | \
     BITS(chip_id, 15, 15) << 7)

#define RDRAM_REG_RASINTERVAL_MAKE(row_precharge, row_sense, row_imp_restore, row_exp_restore) \
    ((BITSWAP5(row_precharge) << 24) | (BITSWAP5(row_sense) << 16) | (BITSWAP5(row_imp_restore) << 8) | (BITSWAP5(row_exp_restore) << 0))


#define INIT_VALUE_OFF   (rdram_init_values - rdram_init_regs)


stage1:
    addiu $v0, dmem_base, %lo((rdram_init_regs - stage1) + (0xA4001000 - 0xA4000040))
.Lrdram_init_loop:
    lw decomp_return, 0($v0)
    addiu $v0, 4
    beqz decomp_return, .Lrdram_init_end
     lw $gp, %lo(INIT_VALUE_OFF-4)($v0)
    sw $gp, 0(decomp_return)
    # Some delay is required after some of the writes of the initialization
    # routine. We always delay as it doesn't matter, and do that with
    # a series of nops that compress very well.
    .rept 128
        nop
    .endr
    b .Lrdram_init_loop
.Lrdram_init_end:

    # Purge the CPU cache. The cache at boot can contain random values in both
    # the contents and the metadata. We have already used dcache in stage 0 (though
    # not trusting its initial contents), but now we want to make sure to boot
    # C code in stage 2 with a fully inited state, including icache (icache
    # boot garbage can cause crashes).
     mtc0 $zero, C0_TAGLO
    mtc0 $zero, C0_TAGHI
    li $t0, 0x80000000
    addiu $t1, $t0, 16384
.Lcache_purge_loop:
    cache INDEX_STORE_TAG_D, 0($t0)
    cache INDEX_STORE_TAG_I, 0($t0)
    addiu $t0, 16
    blt $t0, $t1, .Lcache_purge_loop
    
    # We now need to decompress and run Stage 2 (the intro). To do so:
    #  outbuf = 0xA0000000 = uncached address of RDRAM where to write the intro
    #  outbuf_end = 0xA400xxxx = leftover from stage1 decompression, still works fine as "unbounded" decompression
    #                            (decompression will stop at the end of the compressed stream anyway)
    #  decomp_return = stage2 entrypoint address. This is a relocation because we allow swizzle
    #                  to move the entrypoint if needed to improve compression ratio.
    addiu $ra, dmem_base, (0xa4000054 - 0xa4000040)
    lw decomp_return, 0($v0)
    jr $ra
     li outbuf, 0xA0000000



#define INITID         0x1F

.macro chip_init_regs   chip_id
    .if (\chip_id < NUM_RDRAM_BANKS*2)
        .long RDRAM_REGS(INITID)   + RDRAM_REG_DEVICE_ID
        .long RDRAM_REGS(\chip_id) + RDRAM_REG_MODE
        .long RDRAM_REGS(\chip_id) + RDRAM_REG_RAS_INTERVAL
    .endif
.endm

.macro chip_init_values chip_id
    .if (\chip_id < NUM_RDRAM_BANKS*2)
        .long RDRAM_REG_DEVICE_ID_MAKE(\chip_id)
        .long RDRAM_REG_MODE_DE | RDRAM_REG_MODE_AS | 0x40000000 | RDRAM_REG_MODE_CC(0x30)
        .long RDRAM_REG_RASINTERVAL_MAKE(1, 7, 10, 4)
    .endif
.endm

rdram_init_regs:
    .long RI_CONFIG                                     // 0
    .long RI_CURRENT_LOAD                               // 1
    .long RI_SELECT                                     // 2
    .long RI_REFRESH                                    // 3
    .long RI_MODE                                       // 4
    .long RI_MODE                                       // 5
    .long MI_MODE                                       // 6
    .long RDRAM_REGS_BROADCAST + RDRAM_REG_DELAY        // 7
    .long RDRAM_REGS_BROADCAST + RDRAM_REG_DEVICE_ID    // 8
    .long RDRAM_REGS_BROADCAST + RDRAM_REG_REF_ROW      // 9
    chip_init_regs 0
    chip_init_regs 2
    chip_init_regs 4
    chip_init_regs 6
    .long RI_REFRESH                                    // 10
    .long PIF_COMMAND                                   // 11
    .long 0                                             // 12 (terminator)
    .long __stage2_entrypoint

    .type rdram_init_regs, @object
    .size rdram_init_regs, . - rdram_init_regs

rdram_init_values:
    .long RI_CONFIG_AUTO_CALIBRATION                                // 0
    .long 0                                                         // 1
    .long RI_SELECT_RX_TX                                           // 2
    .long 0                                                         // 3
    .long RI_MODE_RESET                                             // 4
    .long RI_MODE_STANDARD                                          // 5
    .long MI_WMODE_SET_REPEAT_MODE | MI_WMODE_REPEAT_LENGTH(16)     // 6
    .long ROT16(RDRAM_REG_DELAY_MAKE(5, 7, 3, 1))                   // 7
    .long RDRAM_REG_DEVICE_ID_MAKE(INITID)                          // 8
    .long 0                                                         // 9
    chip_init_values 0
    chip_init_values 2
    chip_init_values 4
    chip_init_values 6
    .long RI_REFRESH_AUTO | RI_REFRESH_OPTIMIZE |  RI_REFRESH_CLEANDELAY(52) \
        | RI_REFRESH_DIRTYDELAY(54) \
        | RI_REFRESH_MULTIBANK((1 << NUM_RDRAM_BANKS) - 1)          // 10
    .long PIF_COMMAND_ACK                                           // 11   
    .long _gp                                                       // 12 (gp value in stage2)

    .type rdram_init_values, @object
    .size rdram_init_values, . - rdram_init_values
