# ============================
# IPL2 loaded the whole ROM ("IPL3") into DMEM. This is what happens:
#
# * Stage 0 decompresses stage 1 from DMEM into CPU Cache.
# * Stage 0 copies decompressed Stage 1 from CPU Cache to IMEM.
# * Stage 0 jumps to IMEM to run Stage 1
# * Stage 1 initializes RDRAM and returns
# * Stage 0 decompresses stage 2 from DMEM into RDRAM
# * Stage 0 jumps to stage 2 in RDRAM
# * Stage 2 is the demo
# ============================

#include "loader.h"

# Value of the $t3 register at IPL3 boot
#define BOOT_T3_VALUE           0xA4000040

# Set to 1 if the ROM ASCII title (8 bytes) can be executed as code,
# without breaking stage 0.
#define RUNNABLE_TITLE          1

#define inbuf           $s1
#define outbuf          $s2
#define outbuf_end      $s0
#define decomp_return   $a3


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

    # PI DOM1 parameters. The first byte is actually unused so we change the
    # common value from 0x80 to 0x11, so that the opcode disasms as "andi"
    # which means that it can also be run if necessary.
    .long 0x31371240            # andi $s7, $t1, 0x1240
post_stage1_decomp:
    # This code is run after stage1 has been decompressed.
    # outbuf now points to the end of the decompressed stage1 in cache.
    # Now copy the decompressed stage1 into IMEM, just after stage0. We can't
    # run stage1 from cache because icache fetches only from RDRAM, not dcache.
    li $a1, 0x80900000      # src
    li $a0, 0xA4000000      # dst
    addiu $a2, $a0, 0x1000
    addiu $ra, $a0, (stage2_decomp - 0xA4000000)    # After stage1, go to stage2_decomp
    addiu $sp, $a0, 0x40               # Stack at 0xA400'0040, hoping it's enough
    #if RUNNABLE_TITLE
    li outbuf_end, 0xB0000000          # (this is for stage2_decomp, see below)
    #else
    b .Lcopy_loop
    #endif
     addiu inbuf, $a0, (__stage12 - 0xA4000000)  # Source pointer: PI address of stage12
    .asciiz "Small64"           # ASCIIZ ROM title (8 bytes, including 0 terminator)
                                # When run, it's a "beql $k1, $t5", plus an invalid opcode
                                # that won't ever be run, since beql will skip it
.Lcopy_loop:
    lw $t0, 0($a1)
    addiu $a1, 4
    sw $t0, 0x1000($a0)
    bne $a0, $a2, .Lcopy_loop   # Always copy 4K. We don't waste an opcode to know the exact size here
     addiu $a0, 4
    jr $a2                      # Jump to stage1 in IMEM


# ============================
# Stage 0
# ============================
# This is the first portion of the ROM that runs. It decompresses stage 1
# (RAM initialization), runs it, then decompresses stage 2 (the demo) and
# runs it.

    .section .stage0, "ax"
    .globl _start

_start:
    # Decompress stage1 into CPU cache. The decompression routine needs 8/16/32
    # bit writes to the destination buffer, and the cache is the only available
    # right now that allows for these. The exact address won't matter (as
    # RDRAM is not initialized yet and we are not going to flush the cache),
    # but we use 0x80002000 as address so that emulators that don't emulate
    # CPU cache stil have RDRAM here.
     li outbuf, 0x80900000             # Destination pointer: CPU cache
    addiu $sp, outbuf, 0x2000         # Decompression also needs the stack: end of CPU cache
    addiu inbuf, $t3, (__stage12 - BOOT_T3_VALUE)  # Source pointer: compressed stage 1 (already loaded in DMEM by IPL2)
    addiu outbuf_end, outbuf, STAGE1_SIZE    # End of destination buffer
    b decompress
     li decomp_return, 0xB0000000            # Post-decompression go to ROM start

stage2_decomp:
    # Stage1 returned the memory size in $v0
    # Prepare for stage2. Stage2 will run from 0x8000'0000
    # NOTE:
    # Both inbuf and outbuf_end have been already set in the header code (post_stage1_decomp).
    # outbuf_end = 0xB000'0000 (acts as "infinite", it will stop when it's finished anyway)
    # inbuf = DMEM address of compressed stage12.
    li outbuf, 0xA0000000         # Decompression output pointer (uncached address)
    li decomp_return, 0x80000000     # Stage2 entrypoint (cached address)
    addu $sp, $v0, decomp_return     # $sp start at 0x8000'0000+memsize
    addiu decomp_return, STAGE1_SIZE
#if !RUNNABLE_TITLE
    li outbuf_end, 0xB0000000        # Set outbuf_end here if it couldn't fit the header
#endif
    #
    # Fallthrough to decompression code
    #

#if COMPRESSION_ALGO == 1

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

#define lax_base               $t3
#define offset                 $t4
#define ra2                    $t5
#define ctx                    $t6
#define state                  $t7
#define prev_was_literal       $t8
#define decoded_bit            $v0
#define number_incr            $k0
#define upkr_decode_number_addr $k1

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

# lax load an address of a label into a register. We use $t3 as a base address
# to calculate the offset, so that the load can be performed as a relative
# offset in one instruction, rather than a 32-bit load in two.
.macro lax reg, address
    addiu \reg, lax_base, \address - _start + 0x40
.endm

# Main UPKR decompression loop
decompress:
    # Init the decompression contexts with 0x80 (50% probability).
    li prev_was_literal, 0x80               # init value
    li state, UPKR_NUM_CONTEXTS             # number of contexts to init
.Lctx_init_loop:
    sub $t1, $sp, state                     # calculate the address of the context
    sb prev_was_literal, 0($t1)             # store 0x80
    addiu state, -1                 
    bnez state, .Lctx_init_loop             # loop until all contexts are initialized
     li lax_base, 0xA4000000                # init lax_base to this segment (so lax works)
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
    addu ctx, decoded_bit                   # merge in decoded bit
    srl prev_was_literal, ctx, 8+UPKR_PARITY_LOG2
    beqz prev_was_literal, .Lliteral        # check if we're done (8 bits decoded)

    # NOTE: this code is shared for both literal and match. In case of literal,
    # prev_was_literal is > 0 (see just above) and the actual value doesn't matter.
    # In case of match, prev_was_literal is < 0 and it is the match counter.
.Lstore_byte:
     sb ctx, (outbuf)                       # store the byte (either literal or copied)
    addiu prev_was_literal, 1               # decrement match counter
    addiu outbuf, 1                         # increment output buffer
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
    # return target (depending on whether the bit was 0 or 1). Take the chance
    # to initialize upkr_decode_number_addr to the address of upkr_decode_number.
    jalr upkr_decode_number_addr, $fp
     sb prob, ($t0)

# Decode an elias gamma value into "prev_was_literal".
# The value is stored *negated*, as that's useful for the caller.
# On exit, return to "ra2"
upkr_decode_number:
    li prev_was_literal, 0
    li number_incr, -1
.Lelias_loop:
    bal upkr_decode_bit
     lax $fp, .Lnumber_end
    bal upkr_decode_bit
     lax $fp, .Lelias_loop_next
    addu prev_was_literal, number_incr
.Lelias_loop_next:
    b .Lelias_loop
     addu number_incr, number_incr
.Lnumber_end:
    jr ra2
     addu prev_was_literal, number_incr


#undef prob

#else

    ###################################################
    # Fallthrough to shrinkler decompression
    ###################################################

#define intervalsize    $t2   
#define intervalvalue   $t3
#define nbits           $t4
#define match_off       $t5
#define prev_match      $t6
#define lit_len         $t7
#define num_len         $t7
#define ctx             $t8
#define parity          $v1
#define match_ptr       $k0

#define ra2             $a1
#define lit_ctx         $a1

#define ROUND_UP(n,d)   (((n) + (d) - 1) / (d) * (d))

#define ADJUST_SHIFT                4
#define NUM_SINGLE_CONTEXTS         1
#define NUM_CONTEXT_GROUPS          4
#define CONTEXT_GROUP_SIZE          256
#define NUM_CONTEXTS                (NUM_SINGLE_CONTEXTS + NUM_CONTEXT_GROUPS * CONTEXT_GROUP_SIZE)

#define CONTEXT_KIND             (0 + 1)
#define CONTEXT_REPEATED         (-1 + 1)
#define CONTEXT_GROUP_OFFSET     ((2<<8) + 1)
#define CONTEXT_GROUP_LENGTH     ((3<<8) + 1)


decompress:
    addiu $t1, $sp, -18
    addiu $sp, $sp, -ROUND_UP(NUM_CONTEXTS*2, 8)-16
    li intervalsize, 0x8000
.Linit_contexts_loop:
    sh intervalsize, 0($t1)
    bne $sp, $t1, .Linit_contexts_loop
     addiu $t1, -2

    lwu intervalvalue, 0(inbuf)
    addiu inbuf, 4
    dsll intervalvalue, 31
    li nbits, 1
    li parity, CONTEXT_KIND*2
    li $v0, 1

.Lliteral:
    addu lit_ctx, parity, $sp
.Lliteral_loop:
    sll $v0, 1
    bal .Llzdecode
     addu ctx, lit_ctx, $v0
    sll prev_match, $v0, 23
    bgez prev_match, .Lliteral_loop

.Lstore_byte:
     sb $v0, 0(outbuf)                           # store 1 byte
    addiu outbuf, 1
    daddiu prev_match, -1                       # check if there are more bytes to copy
    bgtz prev_match, .Lmatch1_loop              # check we went past match_len
     xori parity, 0x200

.Lloop:
    bge outbuf, outbuf_end, .Lend
     li $v0, 0
    bal .Llzdecode
     addu ctx, parity, $sp
    beqz $v0, .Lliteral
     xori $v0, 1

.Lmatch:
    bltzal prev_match, .Llzdecode              # prev_match<0: was literal
     addiu ctx, $sp, CONTEXT_REPEATED*2 
    sll $v0, 31
    addiu ctx, $sp, CONTEXT_GROUP_OFFSET*2
    bgezal $v0, .Llzdecode_number
     addiu $v0, match_off, 2
    addiu match_off, $v0, -2
    beqz match_off, .Lend
     sub match_ptr, outbuf, match_off            # calculate start of match
    bal .Llzdecode_number
     addiu ctx, $sp, CONTEXT_GROUP_LENGTH*2
    move prev_match, $v0                        # store match_len into prev_match
.Lmatch1_loop:                                  # 1-byte copy loop
    lbu $v0, 0(match_ptr)                       # load 1 byte
    b .Lstore_byte
     addiu match_ptr, 1

.Lend:
    jr decomp_return     

###################################################
# Function: .Llzdecode_number
#   Arithmetic decoder of a variable sized number
#
# Input:
#  * ctx: first context to use
# Output:
#  * $v0: decoded number
# Destroy:
#  * $t0, $t1
#  * num_len ($t7) (aliased with all other len variables)
#
###################################################

.Llzdecode_number:
     move ra2, $ra
    li num_len, 0
.Llzdecode_number_len:
    li $v0, 0
    bal .Llzdecode
     addiu ctx, 2*2
    bnez $v0, .Llzdecode_number_len
     addiu num_len, 1
    li $v0, 1
    addiu ctx, -1*2
.Llzdecode_number_value:
    sll $v0, 1
    bal .Llzdecode
     addiu num_len, -1
    bnez num_len, .Llzdecode_number_value
     addiu ctx, -2*2
.Llzdecode_number_end:
    jr ra2
     nop

###################################################
# Function: .Llzdecode
#   Arithmetic decoder of one bit
#
# Input: 
#   * ctx: context to use
#   * $v0: value to modify
# Return:
#   * $v0: set last bit to 1 if dcoded bit is 1,
#          otherwise leave it unchanged
# Destroy:
#   * $t0, $t1
###################################################

    .set noat
.Llzdecode:
    sll intervalsize, 16
    bltz intervalsize, .Llzdecode_prob
     lhu $t0, 0(ctx)
.Llzdecode_bit_loop:
    bnez nbits, .Llzdecode_next_bit
     addiu nbits, -1
    lwu $at, 0(inbuf)
    addiu inbuf, 4
    or intervalvalue, $at
    li nbits, 31
.Llzdecode_next_bit:     
    sll intervalsize, 1
    bgez intervalsize, .Llzdecode_bit_loop
     dsll intervalvalue, 1
.Llzdecode_prob:
    dsrl $at, intervalvalue, 48
    multu intervalsize, $t0
    srl intervalsize, 16
    mfhi $t1

    sub $at, $t1
    bgez $at, .Llzdecode_prob_false
     srl $at, $t0, ADJUST_SHIFT

.Llzdecode_prob_true:
    move intervalsize, $t1
    sub $t0, $at
    addiu $t0, (0xffff >> ADJUST_SHIFT)
    sh $t0, 0(ctx)
    jr $ra
     ori $v0, 1

.Llzdecode_prob_false:
    sub intervalsize, $t1
    dsll $t1, $t1, 48
    dsubu intervalvalue, $t1
    sub $t0, $at
    jr $ra
     sh $t0, 0(ctx)
    .set at

#endif