###########################################################################
# Minimal N64 RDRAM initialization code
# Written by Giovanni Bajo (giovannibajo@gmail.com)
###########################################################################
#
# This is loosely based on rdram.c as found in libdragon's IPL3 but it is
# heavily stripped down to the bare minimum, by cutting lots of shortcuts.
# These are the main changes in the initialization algorithm:
#
#  * The current calibration does raise the output current of chips until
#    the read data is stable, and then double that value to set the current.
#    The real IPL3 in libdragon does a weighted sum of the number of 1 bits
#    in the read data, and then multiply that by 2.2. 
#  * Automatic current tuning is not enabled. RAMBUS suggests to enable that
#    do deal with temperature changes, but 4K intros will run for a few minutes
#    at most, so we can hopefully skip that (and save a lot of code).
#  * The RAS interval is set to a fixed value that is expected to work on all
#    RDRAM chips shipped with the N64. Otherwise, querying the chip model would
#    be necessary.
#  * RDRAM refresh rate is not configured. The default appears to work correctly
#    and doesn't seem to impact memory bandwidth that much, as the n64brew wiki
#    implies
#  * The code is configured at compile-time for either 4, 6 or 8 MiB of RDRAM,
#    but does not handle runtime errors. So if configure for 8 Mib, the expansion
#    pak must be present, otherwise the code will hang.
#
###########################################################################


# Define how many banks must be initialized. Each bank is 2 MiB of RAM.
# So 2 banks means 4 MiB, or 4 banks means 8 MiB.
# Notice that there is no error detection so if you ask for 8 MiB, the
# code will hang if expansion pak is not present.
#define NUM_RDRAM_BANKS           2

    .set noreorder
    .set noat

    .globl mini_rdram_init
    .globl mini_rdram_calibrate_current
    .section .text.stage1, "ax"

#define BIT(x, n)           (((x) >> (n)) & 1)
#define BITS(x, b, e)       (((x) >> (b)) & ((1 << ((e)-(b)+1))-1))
#define BITSWAP5(x)         ((BIT(x,0)<<4) | (BIT(x,1)<<3) | (BIT(x,2)<<2) | (BIT(x,3)<<1) | (BIT(x,4)<<0))

#define RDRAM_CHIP          $t5
#define RDRAM_BROADCAST     $t6
#define MI_BASE             $t7
#define RI_BASE             $t8
#define RI_MODE             0x0(RI_BASE)
#define RI_CONFIG           0x4(RI_BASE)
#define RI_CURRENT_LOAD     0x8(RI_BASE)
#define RI_SELECT           0xC(RI_BASE)

#define MI_MODE             0x0(MI_BASE)

#define RI_CONFIG_AUTO_CALIBRATION   0x40
#define RI_SELECT_RX_TX              0x14
#define RI_MODE_STANDARD             (0x8|0x4|0x2)

#define MI_MODE_REPEAT(n)            (0x100 | ((n)-1))

#define RDRAM_REG_DEVICE_ID          (1 * 4)
#define RDRAM_REG_DELAY              (2 * 4)
#define RDRAM_REG_MODE               (3 * 4)
#define RDRAM_REG_REF_ROW            (5 * 4)
#define RDRAM_REG_RAS_INTERVAL       (6 * 4)

#define RDRAM_MODE_DE       (1<<25)
#define RDRAM_MODE_AS       (1<<26)

#define RDRAM_DELAY(AckWinDelay, ReadDelay, AckDelay, WriteDelay) \
   ((((AckWinDelay) & 7) << 3 << 24) | \
    (((ReadDelay)   & 7) << 3 << 16) | \
    (((AckDelay)    & 3) << 3 <<  8) | \
    (((WriteDelay)  & 7) << 3 <<  0))

#define RDRAM_REG_RASINTERVAL_MAKE(row_precharge, row_sense, row_imp_restore, row_exp_restore) \
    ((BITSWAP5(row_precharge) << 24) | (BITSWAP5(row_sense) << 16) | (BITSWAP5(row_imp_restore) << 8) | (BITSWAP5(row_exp_restore) << 0))


#define ROT16(x) ((((x) & 0xFFFF0000) >> 16) | (((x) & 0xFFFF) << 16))

wait:
    li $t0, 0x200
1:  bnez $t0, 1b
     addiu $t0, $t0, -1
    jr $ra

mini_rdram_init:
     li RI_BASE, 0xA4700000
    li MI_BASE, 0xA4300000
    li RDRAM_BROADCAST, 0xA3F80000
    sw $ra, -4($sp)

    li $t0, RI_CONFIG_AUTO_CALIBRATION
    jal wait
     sw $t0, RI_CONFIG
    sw $zero, RI_CURRENT_LOAD

    li $t0, RI_SELECT_RX_TX
    sw $t0, RI_SELECT

    jal wait
     sw $zero, RI_MODE

    li $t0, RI_MODE_STANDARD
    jal wait
     sw $t0, RI_MODE

    li $t0, MI_MODE_REPEAT(16)
    sw $t0, MI_MODE

    li $t0, ROT16(RDRAM_DELAY(5, 7, 3, 1))
    sw $t0, RDRAM_REG_DELAY(RDRAM_BROADCAST)
    
    li $t0, 0xFC800000      # Device ID 0x7F
    sw $t0, RDRAM_REG_DEVICE_ID(RDRAM_BROADCAST)
    sw $zero, RDRAM_REG_REF_ROW(RDRAM_BROADCAST)

    jal mini_rdram_calibrate_current
     li $a0, 0
#if NUM_RDRAM_BANKS > 1
    jal mini_rdram_calibrate_current
     li $a0, 2
#endif
#if NUM_RDRAM_BANKS > 2
    jal mini_rdram_calibrate_current
     li $a0, 4
#endif
#if NUM_RDRAM_BANKS > 3
    jal mini_rdram_calibrate_current
     li $a0, 6
#endif

    lw $ra, -4($sp)
    jr $ra
     li $v0, NUM_RDRAM_BANKS * 2 * 1024 * 1024

    .type mini_rdram_init, @function
    .size mini_rdram_init, .-wait

# Macro to configure the current for the chip.
# We need this function twice: once as part of the loop
# to search for the minimum corrent, and the second time
# to set the current to the calculated value.
# Since this whole module is compressed, it is more efficient
# to simply duplicate the code, rather than abstracting
# it into a function.

.macro set_cc cc
    la $v0, shifts
    li $t3, RDRAM_MODE_DE | RDRAM_MODE_AS | 0x40000000
    xori $t2, \cc, 0x3F
    sll $t2, 32-6
1:
    bgez $t2, 2f
     sll $t2, 1
    lbu $t0, 0($v0)
    li $t1, 1
    sllv $t1, $t1, $t0
    or $t3, $t1
2:
    bnez $t2, 1b
     addiu $v0, 1
    sw $t3, RDRAM_REG_MODE(RDRAM_CHIP)          # write mode reg with CC value
.endm


#define chip_id             $a0
#define RDRAM_CHIP          $t5
#define cc                  $t8
#define vaddr               $t7
#define ra2                 $t6

mini_rdram_calibrate_current:

    # Configure chip ID for the next RDRAM chip in the bus.
    # We previously broadcasted ID 0x7F to all chips, so talk to
    # that ID.
    li RDRAM_CHIP, (0xA3F00000 + (0x7F << 10))
    sll $t0, chip_id, 26
    sw $t0, RDRAM_REG_DEVICE_ID(RDRAM_CHIP)

    li RDRAM_CHIP, 0xA3F00000
    sll chip_id, 10
    or RDRAM_CHIP, chip_id
    
    li vaddr, 0xA0000000
    sll chip_id, 10
    or vaddr, chip_id

    li $t0, (1<<12) | (1<<1) | (1<<2)    # DE=1 => turn on the chip
    sw $t0, RDRAM_REG_MODE(RDRAM_CHIP)

    li cc, 0

.Lcalibrate_loop:
    set_cc cc
    li $t3, 8-1                 # Do 8 RDRAM read tests
    li $t0, 0xFFFFFFFF
.Lrdram_test:
    sw $t0, 0(vaddr)
    sw $t0, 4(vaddr)
    lb $t2, 5(vaddr)
    and $t0, $t2
    bgtz $t3, .Lrdram_test
     addiu $t3, -1

    # Now $t0 is 0xffffffff if all read bits were 1. This is the stopping
    # condition we're looking for
    addiu $t0, 1
    bnezl $t0, .Lcalibrate_loop
     addiu cc, 1

    # Real IPL3 does a weighted sum of the number of 1 bits in the
    # read data, and then multiply that by 2.2. We don't do that, and just
    # double the cc value and use it.
    # We also avoid configuring the auto current mode, as we don't really
    # need to handle temperature changes in the span of a couple of minutes
    # of running time of the intro.
    sll cc, cc, 1
    set_cc cc

    # Configure the RAS interval to a fixed value that is known to work
    # on hopefully all RDRAM chips shipped with the N64.
    li $t0, RDRAM_REG_RASINTERVAL_MAKE(1, 7, 10, 4)
    sw $t0, RDRAM_REG_RAS_INTERVAL(RDRAM_CHIP)

    # Do 8 reads to settle timing circuits
    # The actual opcode is not important, but we repeat the opcode
    # used in the rdram test loop to improve compression.
    lb $t2, 5(vaddr)
    lb $t2, 5(vaddr)
    lb $t2, 5(vaddr)
    lb $t2, 5(vaddr)
    lb $t2, 5(vaddr)
    lb $t2, 5(vaddr)
    lb $t2, 5(vaddr)
    jr $ra
     lb $t2, 5(vaddr)

shifts:
    .byte 23, 15, 7, 22, 14, 6

    .type mini_rdram_calibrate_current, @function
    .size mini_rdram_calibrate_current, .-mini_rdram_calibrate_current

