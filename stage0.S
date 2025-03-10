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

#define clear_cache_addr    $s0

    .set at
    .set noreorder

# ============================
# ROM header
# ============================
# Since most of the header would be unused, we embed the clear_cache function
# in it. This function is called twice by stage 0 so it is a good fit to be
# moved here.
# We still keep a ASCIIZ title so that ROM managers and emulators display
# it correctly.

    .section .header, "ax"

    # PI DOM1 parameters. The first byte is actually unused so we change the
    # common value from 0x80 to 0x11, so that the opcode disasms as "andi"
    # which means that it can also be run if necessary.
#if 1
    .long 0x31371240            # andi $s7, $t1, 0x1240
#else
    .long 0x80371240
#endif
clear_cache:
    mtc0 $0, $28  # taglo
    mtc0 $0, $29  # taghi
    li $t0, 0x80000000
    la $a0, __stage1            # Source pointer for decompression (needed later)
    b .Lclear_cache_loop
    addiu $t1, $t0, 16*1024
    .asciiz "Small64"           # ASCIIZ ROM title. Cannot make it longer...
.Lclear_cache_loop:
    cache (2<<2)|1, 0($t0)     # INDEX_STORE_TAG_D
    cache (2<<2)|0, 0($t0)     # INDEX_STORE_TAG_I
    bne $t0, $t1, .Lclear_cache_loop
     addiu $t0, 16
    jr $ra
     nop


# ============================
# Stage 0
# ============================
# This is the first portion of the ROM that runs. It decompresses stage 1
# (RAM initialization), runs it, then decompresses stage 2 (the demo) and
# runs it.

    .section .stage0, "ax"
    .globl _start

_start:
    # Reset the CPU cache. The status is "invalid" at boot, so better safe
    # than sorry.
    la clear_cache_addr, clear_cache
    jalr clear_cache_addr

    # Decompress stage1 into CPU cache. The decompression routine needs 8/16/32
    # bit writes to the destination buffer, and the cache is the only available
    # right now that allows for these. The exact address won't matter (as
    # RDRAM is not initialized yet and we are not going to flush the cache),
    # but we use 0x80002000 as address so that emulators that don't emulate
    # CPU cache stil have RDRAM here.
    li $a2, 0x80000000             # Destination pointer: CPU cache
    # la $a0, __stage1                # Source pointer: compressed stage 1 (already loaded in DMEM by IPL2)
                                      # This opcode is moved within the clear_cache function
    bal decompress_shrinkler
     addiu $sp, $a2, 0x2000           # Decompression also needs the stack: end of CPU cache

    # a2 now points to the end of the decompressed stage1 in cache.
    # Now copy the decompressed stage1 into IMEM, just after stage0. We can't
    # run stage1 from cache because icache fetches only from RDRAM, not dcache.
    li $a1, 0x80000000      # src
    li $a0, 0xA4000000      # dst
.Lcopy_loop:
    lw $t0, 0($a1)
    addiu $a1, 4
    sw $t0, 0x1000($a0)
    bne $a2, $a1, .Lcopy_loop
     addiu $a0, 4

    # Reset the cache again. We want to avoid the cache to flush back to RDRAM
    # after it has been intialized, so clear it.
    jalr clear_cache_addr
     lui $sp, %hi(STACK1_TOP)

    # Call stage1. Initialize the stack to STACK1_TOP which is dcache again,
    # but it's in the high part of RDRAM where we are going later to put stage2.
    bal _start+0x1000-0x40       # Convoluted way to point to IMEM in a section-relative way
     addiu $sp, %lo(STACK1_TOP)

    # Stage1 returned the memory size in $v0
    # Prepare for stage2. Stage2 will run from 0x8000'0000
    la $a0, __stage2 #- 0xA4000000 + 0xB0000000  # calculate PI address instead of DMEM
    li $a2, 0xA0000000            # Decompression output pointer (uncached address)
    li $ra, 0x80000000            # Stage2 entrypoint (cached address)
    addu $sp, $v0, $ra            # $sp start at 0x8000'0000+memsize

    ###################################################
    # Fallthrough to shrinkler decompression
    ###################################################

#define inbuf           $a0
#define outbuf          $a2

#define intervalsize    $t2   
#define intervalvalue   $t3
#define nbits           $t4
#define match_off       $t5
#define prev_match      $t6
#define lit_len         $t7
#define match_len       $t7
#define num_len         $t7
#define ctx             $t8
#define parity          $v1

#define ra2             $a1
#define lit_ctx         $a1
#define ra3             $a3

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


decompress_shrinkler:
    addiu $t1, $sp, -16
    addiu $sp, $sp, -ROUND_UP(NUM_CONTEXTS*2, 8)-16
    move $t0, $sp
    li $t2, 0x80008000
.Linit_contexts_loop:
    sw $t2, 0($t0)
    sw $t2, 4($t0)
    blt $t0, $t1, .Linit_contexts_loop
     addiu $t0, 8

    move ra3, $ra
    lwu intervalvalue, 0(inbuf)
    addiu inbuf, 4
    dsll intervalvalue, 31
    li nbits, 1
    li intervalsize, 0x8000
    li parity, CONTEXT_KIND*2
    b .Lliteral
     li $v0, 1

.Lloop:
    li $v0, 0
    bal .Llzdecode
     addu ctx, parity, $sp
    bnez $v0, .Lmatch
     xori $v0, 1

.Lliteral:
    li prev_match, 0
    li lit_len, 7
    addu lit_ctx, parity, $sp
.Lliteral_loop:
    sll $v0, 1
    bal .Llzdecode
     addu ctx, lit_ctx, $v0
    bnez lit_len, .Lliteral_loop
     addiu lit_len, -1

    xori parity, 0x200
    sb $v0, 0(outbuf)
    b .Lloop
     addiu outbuf, 1

.Lmatch:
    bgezal prev_match, .Llzdecode
     addiu ctx, $sp, CONTEXT_REPEATED*2 
    sll $v0, 31
    addiu ctx, $sp, CONTEXT_GROUP_OFFSET*2
    bgezal $v0, .Llzdecode_number
     addiu $v0, match_off, 2
    addiu $v0, -2
    beqz $v0, .Lend
     move match_off, $v0
    bal .Llzdecode_number
     addiu ctx, $sp, CONTEXT_GROUP_LENGTH*2
    move match_len, $v0
    andi $t0, match_len, 1
    sll $t0, 9
    xor parity, $t0
    li prev_match, 1<<31
    sub $v0, outbuf, match_off                  # calculate start of match
.Lmatch1_loop:                                  # 1-byte copy loop
    lbu $t0, 0($v0)                             # load 1 byte
    addiu $v0, 1
    sb $t0, 0(outbuf)                           # store 1 byte
    addiu match_len, -1
    bgtz match_len, .Lmatch1_loop               # check we went past match_len
     addiu outbuf, 1
    b .Lloop                                    # jump to main loop
     nop

.Lend:
    jr ra3
     addiu $sp, $sp, ROUND_UP(NUM_CONTEXTS*2, 8)+16

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
