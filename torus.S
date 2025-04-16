    .set noreorder
    .set noat
    .section .text.stage2, "ax", @progbits
    .globl gentorus
    .type gentorus, @function


#define SHIFTXY   7
#define SHIFTZ    2
#define NUM_MAJOR 32
#define NUM_MINOR 16
#define STEP_MAJOR (0x100 / NUM_MAJOR)
#define STEP_MINOR (0x100 / NUM_MINOR)
#define COS_OFFSET 64

#define R          64

#define v          $t0
#define outptr      $t4
#define rcv0        $t5
#define u0          $t7
#define u1          $t8
#define sinv0       $k0
#define cosv0       $k1

.macro gencoord m
    move $v1, \m
    mult $v0, $v1
    mflo $v0
    sra $v0, SHIFTXY
    sb $v0, 0(outptr)
    addiu outptr, outptr, 1
.endm

.macro cos arg
  jal mm_sin_s8
    addiu $a0, \arg, COS_OFFSET
.endm

.macro sin arg
  jal mm_sin_s8
    addiu $a0, \arg, 0
.endm


gentorus:
    addiu $sp, $sp, -16
    sw $ra, 12($sp)
    move outptr, $a0

    move u0, $zero

    # Loop over the torus patches
.Lnext_patch:
    move v, $zero
    li sinv0, 0x00
    li cosv0, 0x7f
    addiu u1, u0, STEP_MAJOR

    # loop over each quad in the current patch
.Lnext_quad:

    sra $v0, cosv0, SHIFTZ
    addiu rcv0, $v0, R

    ######################
    # VTX0
    ######################

    # vtx[0].pos[0]
    cos u0
    gencoord rcv0

    # vtx[0].pos[1]
    sin u0
    gencoord rcv0

    # vtx[0].pos[2]
    sra $v0, sinv0, SHIFTZ
    sb $v0, 0(outptr)
    addiu outptr, outptr, 2

    # vtx[0].normal[0]

    cos u0
    gencoord cosv0

    # vtx[0].normal[1]

    sin u0
    gencoord cosv0

    # vtx[0].normal[2]
    sb sinv0, 0(outptr)
    addiu outptr, outptr, 2

    ######################
    # VTX1
    ######################

    # vtx[1].pos[0]

    cos u1
    gencoord rcv0

    # vtx[1].pos[1]

    sin u1
    gencoord rcv0

    # vtx[1].pos[2]
    sra $v0, sinv0, SHIFTZ
    sb $v0, 0(outptr)
    addiu outptr, outptr, 2

    # vtx[1].normal[0]
    cos u1
    gencoord cosv0

    # vtx[1].normal[1]
    sin u1
    gencoord cosv0

    # vtx[1].normal[2]
    sb sinv0, 0(outptr)
    addiu outptr, outptr, 2

    ######################

    jal mm_sin_s8
     addiu $a0, v, STEP_MINOR + COS_OFFSET
    move cosv0, $v0

    jal mm_sin_s8
     addiu $a0, v, STEP_MINOR
    move sinv0, $v0

    sra $v0, cosv0, SHIFTZ
    addiu rcv0, $v0, R

    ######################
    # VTX2
    ######################

    # vtx[1].pos[0]

    cos u1
    gencoord rcv0

    # vtx[1].pos[1]

    sin u1
    gencoord rcv0

    # vtx[1].pos[2]
    sra $v0, sinv0, SHIFTZ
    sb $v0, 0(outptr)
    addiu outptr, outptr, 2

    # vtx[1].normal[0]
    cos u1
    gencoord cosv0

    # vtx[1].normal[1]
    jal mm_sin_s8
     move $a0, u1
    gencoord cosv0

    # vtx[1].normal[2]
    sb sinv0, 0(outptr)
    addiu outptr, outptr, 2

    ######################
    # VTX3
    ######################

    # vtx[3].pos[0]

    cos u0
    gencoord rcv0

    # vtx[3].pos[1]
    sin u0
    gencoord rcv0

    # vtx[3].pos[2]
    sra $v0, sinv0, SHIFTZ
    sb $v0, 0(outptr)
    addiu outptr, outptr, 2

    # vtx[3].normal[0]
    cos u0
    gencoord cosv0

    # vtx[3].normal[1]

    sin u0
    gencoord cosv0

    # vtx[3].normal[2]
    sb sinv0, 0(outptr)
    addiu outptr, outptr, 2

    ######################
    # VTX4 (copy of VTX0)
    ######################

    ld $v0, -32(outptr)
    sd $v0, 0(outptr)
    addiu outptr, outptr, 8

    ######################
    # VTX5 (copy of VTX2)
    ######################

    ld $v0, -24(outptr)
    sd $v0, 0(outptr)
    addiu outptr, outptr, 8

    # quad loop tail

    li $a0, 0x100
    addiu v, v, STEP_MINOR
    bne v, $a0, .Lnext_quad
     nop

    # patch loop tail
    addiu u0, u0, STEP_MAJOR
    bne u0, $a0, .Lnext_patch
     nop

    # done

    lw $ra, 12($sp)
    jr $ra
     addiu $sp, $sp, 16

.size gentorus, .-gentorus