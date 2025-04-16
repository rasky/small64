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
#define end         $t1
#define outptr      $t4
#define rcv0        $t5
#define u0          $t7
#define u1          $t8
#define sinv0       $k0
#define cosv0       $k1

.macro gencoord m
    addiu $v1, \m, 0
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

.macro do_vert arg
    # vtx.pos[0]
    cos \arg
    gencoord rcv0

    # vtx.pos[1]
    sin \arg
    gencoord rcv0

    # vtx.pos[2]
    sra $v0, sinv0, SHIFTZ
    sb $v0, 0(outptr)
    addiu outptr, outptr, 2

    # vtx.normal[0]
    cos \arg
    gencoord cosv0

    # vtx.normal[1]
    sin \arg
    gencoord cosv0

    # vtx.normal[2]
    sb sinv0, 0(outptr)
    addiu outptr, outptr, 2
.endm

gentorus:
    addiu $sp, $sp, -16
    sw $ra, 12($sp)
    addiu outptr, $a0, 0
    li end, 0x100

    li u0, 0

    # Loop over the torus patches
    # do {
.Lnext_patch:
    li v, 0
    li sinv0, 0
    li cosv0, 0x7f
    addiu u1, u0, STEP_MAJOR

    # int rcv0 = R + (cosV0 >> shiftZ);
    sra $v0, cosv0, SHIFTZ
    addiu rcv0, $v0, R

    # loop over each quad in the current patch
    # do {
.Lnext_quad:

    do_vert u0 # write vtx[0]
    do_vert u1 # write vtx[1]

    # cosV0 = mm_cos_s8(v0 + stepMinor);
    jal mm_sin_s8
     addiu $a0, v, STEP_MINOR + COS_OFFSET
    addiu cosv0, $v0, 0

    # sinV0 = mm_sin_s8(v0 + stepMinor);
    jal mm_sin_s8
     addiu $a0, v, STEP_MINOR
    addiu sinv0, $v0, 0

    # int rcv0 = R + (cosV0 >> shiftZ);
    sra $v0, cosv0, SHIFTZ
    addiu rcv0, $v0, R

    do_vert u1 # write vtx[2]
    do_vert u0 # write vtx[3]

    # vtx[4] = vtx[0]
    ld $v0, -32(outptr)
    sd $v0, 0(outptr)
    addiu outptr, outptr, 8

    # vtx[5] = vtx[2]
    ld $v0, -24(outptr)
    sd $v0, 0(outptr)
    addiu outptr, outptr, 8

    # quad loop tail
    # } while (++v != end);
    addiu v, v, STEP_MINOR
    bne v, end, .Lnext_quad
      nop

    # patch loop tail
    # } while (++u0 != end);
    addiu u0, u0, STEP_MAJOR
    bne u0, end, .Lnext_patch
      nop

    # done

    lw $ra, 12($sp)
    jr $ra
     addiu $sp, $sp, 16

.size gentorus, .-gentorus