###########################################################################
# Stage0: bootstrap for a compressed 4K demo
# Written by Giovanni Bajo (giovannibajo@gmail.com)
###########################################################################
# This code is written so that:
#
#  * The minimum possible amount of code is not compressed. That includes
#    the upkr decompressor.
#  * The maximum amount of code is stuffed into the ROM header (0x40) so
#    to use that area of the ROM as well.
#
# IPL2 loaded the whole 4K ROM (that it believes it is the IPL3) into DMEM.
# This is what happens:
#
# * Stage 0 decompresses Stage 1 from DMEM into IMEM
# * Stage 0 jumps to IMEM to run Stage 1
# * Stage 1 initializes RDRAM and returns
# * Stage 0 decompresses stage 2 from DMEM into RDRAM
# * Stage 0 jumps to stage 2 in RDRAM
# * Stage 2 is the demo
###########################################################################

# Value of the $t3 register at IPL3 boot
#define BOOT_T3_VALUE           0xA4000040

#define inbuf           $s1
#define outbuf          $s2
#define outbuf_end      $s0
#define decomp_return   $a3
#define dmem_base       $t3         // Base address for lax macro (set by IPL2)

#define offset                 $t4
#define ra2                    $t5
#define ctx                    $t6
#define state                  $t7
#define prev_was_literal       $t8
#define decoded_bit            $v0
#define number_incr            $k0
#define writeword              $v1
#define upkr_decode_number_addr $k1
#define upkr_decode_bit_addr   $s3

# lax load an address of a label into a register. We use $t3 as a base address
# to calculate the offset, so that the load can be performed as a relative
# offset in one instruction, rather than a 32-bit load in two.
.macro lax reg, address
    addiu \reg, dmem_base, \address - _start
.endm

# Convert a DMEM label into the address of the same function in ROM
#define DMEM_TO_ROM(addr)       (header_start + (addr - _start) + 0x40)

    .set at
    .set noreorder

# ============================
# ROM header
# ============================
# Since most of the header would be unused, we embed a function to copy the
# decompressed stage1 from cache to IMEM in the header.
# We still keep a ASCIIZ title so that ROM managers and emulators display
# it correctly, plus the PI DOM1 parameters, which are used by the IPL2
# to load the ROM into DMEM.

    .section .header, "ax"
header_start:

    # PI DOM1 parameters. The first byte is actually unused so we change the
    # common value from 0x80 to 0x11, so that the opcode disasms as "andi"
    # which means that it can also be run if necessary.
    .long 0x31371240            # andi $s7, $t1, 0x1240

# Decode an elias gamma value into "prev_was_literal".
# The value is stored *negated*, as that's useful for the caller.
# On exit, return to "ra2"
upkr_decode_number:
    # Load the address of the upkr_decode_bit address in DMEM. We could 
    # instead run it from ROM but then the whole decompression becomes quite slow.
    # Since we have an extre free word here in the header, let's use it this way.
    lax upkr_decode_bit_addr, upkr_decode_bit
    li prev_was_literal, 0
    li number_incr, -1
.Lelias_loop:
    jalr upkr_decode_bit_addr
     addiu $fp, upkr_decode_number_addr, (.Lnumber_end - header_start)
    jalr upkr_decode_bit_addr
     addiu $fp, upkr_decode_number_addr, (.Lelias_loop_next - header_start)
    .asciiz "Small64"           # ASCIIZ ROM title (8 bytes, including 0 terminator)
                                # When run, it's a "beql $k1, $t5", plus an invalid opcode
                                # that won't ever be run, since beql will skip it
    addu prev_was_literal, number_incr
.Lelias_loop_next:
    b .Lelias_loop
     addu number_incr, number_incr
.Lnumber_end:
    jr ra2
     addu prev_was_literal, number_incr
    
    # We respect the last word in the header, so that we can encode the video
    # type. This is just advisory, but it allows emulators to self-configure
    # correctly when running our ROMs.
    .byte 0
    .byte 0
#if VIDEO_TYPE == 0 // PAL
    .byte 'X'
#elif VIDEO_TYPE == 1 // NTSC
    .byte 'E'
#elif VIDEO_TYPE == 2 // MPAL
    .byte 'B'
#endif
    .byte 0

# ============================
# Stage 0
# ============================
# This is the first portion of the ROM that runs. It decompresses stage 1
# (RAM initialization), runs it, then decompresses stage 2 (the demo) and
# runs it.

    .section .stage0, "ax"
    .globl _start

_start:
    # Set output pointer for destination buffer to IMEM. This is a convoluted
    # way to do it as an offset from dmem_base so that we only do that in one opcode.
    addiu outbuf, dmem_base, (0xA4001000 - BOOT_T3_VALUE)

    # Configure the stack pointer in CPU cache. This is also the final value for $sp
    # that will be used when the demo boots, so that we don't need to change it anymore.
    li $sp, 0x80400000

    # Configure the end pointer for decompression, so that the decompression routine
    # exits when it has finished unpacking Stage 1.
    addiu outbuf_end, outbuf, STAGE1_SIZE

    # We put upkr_decode_number function in the header, we need an address to run
    # it from there.
    li upkr_decode_number_addr, 0xB0000000

    # At the end of decompression, run the stage1 from IMEM.
    move decomp_return, outbuf

    ###################################################
    # upkr decompressor
    ###################################################
    #
    # This decompressor support parity (1, 2, or 4), by changing the UPKR_PARITY
    # value. The support comes at the cost of extra stack space, but requires
    # no extra opcodes.
    ###################################################

#define UPKR_PARITY            4
#define UPKR_PARITY_LOG2       (UPKR_PARITY/2)     // works for 1,2,4
#define UPKR_NUM_CONTEXTS      (256*UPKR_PARITY + 64*2*UPKR_PARITY)

.equ hex.$zr, 0;  .equ hex.$at, 1;  .equ hex.$v0, 2;  .equ hex.$v1, 3
.equ hex.$a0, 4;  .equ hex.$a1, 5;  .equ hex.$a2, 6;  .equ hex.$a3, 7
.equ hex.$t0, 8;  .equ hex.$t1, 9;  .equ hex.$t2, 10; .equ hex.$t3, 11
.equ hex.$t4, 12; .equ hex.$t5, 13; .equ hex.$t6, 14; .equ hex.$t7, 15
.equ hex.$s0, 16; .equ hex.$s1, 17; .equ hex.$s2, 18; .equ hex.$s3, 19
.equ hex.$s4, 20; .equ hex.$s5, 21; .equ hex.$s6, 22; .equ hex.$s7, 23
.equ hex.$t8, 24; .equ hex.$t9, 25; .equ hex.$k0, 26; .equ hex.$k1, 27
.equ hex.$gp, 28; .equ hex.$sp, 29; .equ hex.$fp, 30; .equ hex.$ra, 31

# Run a bltz encoded in a way that can be used in a delay slot of another
# branch. prev_branch is the target of the previous branch. Notice that the
# first opcode there will always be run as delay slot of *this* bltz.
.macro delay_bltz reg, label, prev_branch
    .long (1<<26) | (hex.\reg << 21) | (0 << 16) | ((\label - \prev_branch) >> 2)
.endm

# Main UPKR decompression loop
upkr_decompress:
    # Source pointer: compressed data stream (already loaded in DMEM by IPL2)
    addiu inbuf, dmem_base, (__stage12 - BOOT_T3_VALUE)  

    # Init the decompression contexts with 0x80 (50% probability).
    li prev_was_literal, 0x80               # init value
    li state, UPKR_NUM_CONTEXTS             # number of contexts to init
.Lctx_init_loop:
    sub $t1, $sp, state                     # calculate the address of the context
    addiu state, -1                 
    bnez state, .Lctx_init_loop             # loop until all contexts are initialized
     sb prev_was_literal, 0($t1)             # store 0x80
    # state now is 0
    # prev_was_literal now is non-zero

.Lmainloop:
    bge outbuf, outbuf_end, .Lexit          # check if we reached the end of the output buffer
     andi ctx, outbuf, UPKR_PARITY-1        # calculate the context index (depending on output buffer and parity)
    bal upkr_decode_bit                     # decode a bit: 1->match, 0->literal
     lax $fp, .Lliteral
    
    beqz prev_was_literal, .Lread_offset    # if prev was a literal, we must read an offset
     li ctx, 256*UPKR_PARITY
    bal upkr_decode_bit                     # decode a bit: 1->new offset 0->reuse offset
     lax $fp, .Lgot_offset

.Lread_offset:
    jalr ra2, upkr_decode_number_addr       # decode match offset into "prev_was_literal"
     li ctx, (256+1)*UPKR_PARITY            # (it is decoded as negative)
    addiu offset, prev_was_literal, 1       # subtract 1 (offset is encoded as +1)

.Lgot_offset:                               # we have match offset in "offset"
    jalr ra2, upkr_decode_number_addr       # decode match length (negative)
     li ctx, (256+64)*UPKR_PARITY
    beqz offset, .Lexit                     # if offset is 0, it's EOF. Notice we should
                                            # check this before decoding the length
                                            # but we save an opcode this way
                                    
.Lcopy_byte:                                # copy loop
     addu $t0, outbuf, offset               # calculate the current start of the match    
    b .Lstore_byte
     lbu ctx, ($t0)                         # read a byte to copy and jump to store

.Lliteral:                                  # literal decoding
                                            # ctx starts at UPKR_PARITY (1, 2 or 4) | parity_lsb
    bal upkr_decode_bit                     # decode next bit of literal
     move $fp, $ra
    addiu ctx, -UPKR_PARITY                 # reverse the increment by upkr_decode_bit
    sll ctx, 1                              # make room for the new bit
    srl prev_was_literal, ctx, 8+UPKR_PARITY_LOG2
    beqz prev_was_literal, .Lliteral        # check if we're done (8 bits decoded)
     addu ctx, decoded_bit                   # merge in decoded bit

    # NOTE: this code is shared for both literal and match. In case of literal,
    # prev_was_literal is > 0 (see just above) and the actual value doesn't matter.
    # In case of match, prev_was_literal is < 0 and it is the match counter.
.Lstore_byte:
    # NOTE: this sequence allows to perform correct 8-bit writes to IMEM, which
    # are necessary for upkr to work correctly. A simple "sb" would not work.
    sll writeword, 8                        # make space for the new byte
    andi ctx, 0xFF                          # isolate the byte
    or writeword, ctx                       # merge in the new byte
    sb writeword, (outbuf)                  # store the byte (either literal or copied)
    addiu outbuf, 1                         # increment output buffer
    addiu prev_was_literal, 1               # decrement match counter
    b .Lmainloop                            # prev_was_literal<0? continue copy, otherwise jump back to main loop
     delay_bltz prev_was_literal, .Lcopy_byte, .Lmainloop

.Lexit:
    jr decomp_return                        # time to exit

# upkr_load_byte
#  Shift a byte from the input buffer into "state"
upkr_load_byte:
     lbu decoded_bit, 0(inbuf)
    addiu inbuf, 1
    sll state, 8
    addu state, decoded_bit
    # fallthrough to upkr_decode_bit

# upkr_decode_bit:
#   Decode a single bit from the rANS bytestream into "decoded_bit"
#   On exit, jump to $ra if the bit is 1, and to $fp if the bit is zero.
#define prob   $t1
upkr_decode_bit:
    srl decoded_bit, state, 12
    beqz decoded_bit, upkr_load_byte
     andi $t2, state, 0xff

    addiu ctx, UPKR_PARITY
    subu $t0, $sp, ctx
    lbu prob, 0($t0)

    sltu decoded_bit, $t2, prob
    bnezl decoded_bit, 1f
     neg prob
1:  
    andi prob, 0xff
    srl $t2, state, 8
    addiu $t2, $t2, 1
    subu $t2, decoded_bit
    mult $t2, prob
    mflo $t2
    subu state, $t2

    addiu $t2, prob, 8
    srl $t2, 4
    beqz decoded_bit, 1f
     subu prob, $t2
    neg prob
    move $fp, $ra
1:
    # Store the updated probability and return. $fp now contains the correct
    # return target (depending on whether the bit was 0 or 1).
    jr $fp
     sb prob, ($t0)


#undef prob


###########################################################################
# Stage 1: minimal N64 RDRAM initialization code
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

    .section .stage1, "ax"
    .globl stage1

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
    # Convoluted way of loading the address of rdram_init_regs into $v0,
    # via dmem_base.
    addiu $v0, dmem_base, %lo((rdram_init_regs - stage1) + (0xA4001000 - 0xA4000040))

    # Do RDRAM register initialization. For each loop, we load the register address
    # into decomp_return, and the value into $gp, and then store it.
    # The table is setup so that at the end, when decomp_return is 0, $gp will
    # contain the real value that it must have during stage2 (_gp linker script symbol).
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
    #  $ra = upx_decompress = address of the decompressor to run (from DMEM)
     lax $ra, upkr_decompress
    li outbuf, 0xA0000000
    lw decomp_return, 0($v0)
    addiu $v0, 4

    # Check if RDRAM is working properly. We initialized it with a fixed current
    # value of 0x20. If it appears not to be working, we will switch to a fixed
    # value of 0x30 as last ditch.
    li $t0, 0xFFFFFFFF
    sw $t0, (outbuf)
    nop; nop; nop; nop
    lw $t1, (outbuf)
    beq $t0, $t1, .Lexit_stage1
    
    # Switch current to 0x30
    lw $t0, 0($v0)
    lw $t1, %lo(INIT_VALUE_OFF)($v0)
    sw $t1, 0($t0)

.Lexit_stage1:
    jr $ra
     nop
     



#define INITID         0x1F

.macro chip_init_regs   chip_id
    .if (\chip_id < NUM_RDRAM_BANKS*2)
        .long RDRAM_REGS(INITID)   + RDRAM_REG_DEVICE_ID
        #.long RDRAM_REGS(\chip_id) + RDRAM_REG_MODE
        #.long RDRAM_REGS(\chip_id) + RDRAM_REG_RAS_INTERVAL
    .endif
.endm

.macro chip_init_values chip_id
    .if (\chip_id < NUM_RDRAM_BANKS*2)
        .long RDRAM_REG_DEVICE_ID_MAKE(\chip_id)
        #.long RDRAM_REG_MODE_DE | RDRAM_REG_MODE_AS | 0x40000000 | RDRAM_REG_MODE_CC(0x30)
        #.long RDRAM_REG_RASINTERVAL_MAKE(1, 7, 10, 4)
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
    .long RDRAM_REGS_BROADCAST + RDRAM_REG_MODE
    .long RDRAM_REGS_BROADCAST + RDRAM_REG_RAS_INTERVAL
    chip_init_regs 0
    chip_init_regs 2
    chip_init_regs 4
    chip_init_regs 6
    .long RI_REFRESH                                    // 10
    .long PIF_COMMAND                                   // 11
    .long 0                                             // 12 (terminator)
    .long __stage2_entrypoint                           // 13 (stage2 entrypoint)
    .long RDRAM_REGS_BROADCAST + RDRAM_REG_MODE         // 14 (optional: change current to 0x30)

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
    .long RDRAM_REG_MODE_DE | RDRAM_REG_MODE_AS | 0x40000000 | RDRAM_REG_MODE_CC(0x20)
    .long RDRAM_REG_RASINTERVAL_MAKE(1, 7, 10, 4)
    chip_init_values 0
    chip_init_values 2
    chip_init_values 4
    chip_init_values 6
    .long RI_REFRESH_AUTO | RI_REFRESH_OPTIMIZE |  RI_REFRESH_CLEANDELAY(52) \
        | RI_REFRESH_DIRTYDELAY(54) \
        | RI_REFRESH_MULTIBANK((1 << NUM_RDRAM_BANKS) - 1)          // 10
    .long PIF_COMMAND_ACK                                           // 11   
    .long _gp                                                       // 12 (gp value in stage2)
    .long 0                                                         // 13 (unused)
    .long RDRAM_REG_MODE_DE | RDRAM_REG_MODE_AS | 0x40000000 | RDRAM_REG_MODE_CC(0x30) // 14 (optional: change current to 0x30)

    .type rdram_init_values, @object
    .size rdram_init_values, . - rdram_init_values
