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

#define outptr      $t0
#define u0          $t1
#define u1          $t3
#define v           $t6

#define rcv0        $t4
#define cosv0       $t5
#define sinv0       $at
#define end         $a2

#define ret_addr    $a3

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
    addiu outptr, outptr, 1
    addiu outptr, outptr, 1

    # vtx.normal[0]
    cos \arg
    gencoord cosv0

    # vtx.normal[1]
    sin \arg
    gencoord cosv0

    # vtx.normal[2]
    addiu $v0, sinv0, 0
    sb $v0, 0(outptr)
    addiu outptr, outptr, 1
    addiu outptr, outptr, 1
.endm

    # gentorus - generate torus vertex/polygon data
    # The C version is commented in mesh.c.
    # The assembly version has been written to be able to write compressible
    # code. Each vertex and polygon uses highly repeated code, always using the
    # same opcodes with the same registers, so that it compresses much more.
    # GCC instead generates more efficient code using more registers, but that
    # compresses much worse.
gentorus:
    addiu ret_addr, $ra, 0
    addiu outptr, $a0, 0

    li u0, 0
    li end, 0x100

    # Loop over the torus patches
    # do {
.Lnext_patch:
    li v, 0
    li sinv0, 0
    li cosv0, 0x7f

    # int rcv0 = R + (cosV0 >> shiftZ);
    sra $v0, cosv0, SHIFTZ
    addiu rcv0, $v0, R
    addiu u1, u0, STEP_MAJOR

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
    addiu outptr, outptr, 16

    # vtx[5] = vtx[2]
    # (use suboptimal offset for store because we can reuse the load offset)
    ld $v0, -32(outptr)
    sd $v0, -8(outptr)

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
    jr ret_addr
      nop

.size gentorus, .-gentorus