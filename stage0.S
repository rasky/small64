# ============================
# IPL2 loaded the whole ROM ("IPL3") into DMEM. This is what happens:
#
# * Stage 0 decompresses stage 1 from DMEM into IMEM
# * Stage 0 jumps to IMEM to run Stage 1
# * Stage 1 initializes RDRAM and returns
# * Stage 0 decompresses stage 2 from DMEM into RDRAM
# * Stage 0 jumps to stage 2 in RDRAM
# * Stage 2 is the demo
# ============================

# Value of the $t3 register at IPL3 boot
#define BOOT_T3_VALUE           0xA4000040

# Set to 1 if the ROM ASCII title (8 bytes) can be executed as code,
# without breaking stage 0.
#define RUNNABLE_TITLE          1

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

    # Source pointer: compressed data stream (already loaded in DMEM by IPL2)
    addiu inbuf, dmem_base, (__stage12 - BOOT_T3_VALUE)  

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
decompress:
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
