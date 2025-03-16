## Auto-generated file, transpiled with RSPL
#define RSPQ_BeginOverlayHeader ;
#define RSPQ_EndOverlayHeader ;
#define RSPQ_BeginSavedState ;
#define RSPQ_EndSavedState ;
#define TRI_COMMAND 0xCF00
#include <rsp.inc>

.set noreorder
.set noat
.set nomacro

#undef zero
#undef at
#undef v0
#undef v1
#undef a0
#undef a1
#undef a2
#undef a3
#undef t0
#undef t1
#undef t2
#undef t3
#undef t4
#undef t5
#undef t6
#undef t7
#undef s0
#undef s1
#undef s2
#undef s3
#undef s4
#undef s5
#undef s6
#undef s7
#undef t8
#undef t9
#undef k0
#undef k1
#undef gp
#undef sp
#undef fp
#undef ra
.equ hex.$zero, 0
.equ hex.$at, 1
.equ hex.$v0, 2
.equ hex.$v1, 3
.equ hex.$a0, 4
.equ hex.$a1, 5
.equ hex.$a2, 6
.equ hex.$a3, 7
.equ hex.$t0, 8
.equ hex.$t1, 9
.equ hex.$t2, 10
.equ hex.$t3, 11
.equ hex.$t4, 12
.equ hex.$t5, 13
.equ hex.$t6, 14
.equ hex.$t7, 15
.equ hex.$s0, 16
.equ hex.$s1, 17
.equ hex.$s2, 18
.equ hex.$s3, 19
.equ hex.$s4, 20
.equ hex.$s5, 21
.equ hex.$s6, 22
.equ hex.$s7, 23
.equ hex.$t8, 24
.equ hex.$t9, 25
.equ hex.$k0, 26
.equ hex.$k1, 27
.equ hex.$gp, 28
.equ hex.$sp, 29
.equ hex.$fp, 30
.equ hex.$ra, 31
#define vco 0
#define vcc 1
#define vce 2

.data
  RSPQ_BeginOverlayHeader
  RSPQ_EndOverlayHeader

  RSPQ_BeginSavedState
    STATE_MEM_START:
    SHIFT_DATA: .ds.b 8
    .align 2
    VERTEX_ADDR: .ds.b 4
    .align 2
    VERTEX_ADDR_END: .ds.b 4
    .align 1
    SCREEN_OFFSET: .ds.b 4
    .align 3
    MATRIX: .ds.b 24
    .align 4
    RSPQ_DMEM_BUFFER: .ds.b 128
    .align 4
    TRI_BUFF: .ds.b 192
    .align 4
    VERT_BUFF: .ds.b 32
    .align 4
    RSPQ_SCRATCH_MEM: .ds.b 16
    STATE_MEM_END:
  RSPQ_EndSavedState

.text
OVERLAY_CODE_START:

Main:
  vxor $v00, $v00, $v00 # inline-ASM
  lpv $v31, 0, 0, $zero
  vmudl $v30, $v31, $v31.e7
  lw $s6, %lo(VERTEX_ADDR + 0)
  lw $s5, %lo(VERTEX_ADDR_END + 0)
  MAIN_LOOP:
  ori $t0, $zero, %lo(VERT_BUFF)
  addiu $t1, $zero, 24
  mtc0 $t0, COP0_DMA_SPADDR ## Barrier: 0x1
  mtc0 $s6, COP0_DMA_RAMADDR ## Barrier: 0x1
  mtc0 $t1, COP0_DMA_READ ## Barrier: 0x1
  addu $s6, $s6, $t1
  addu $t2, $t0, $t1
  vxor $v01, $v00, $v31.e6
  lsv $v01, 0, 30, $zero
  lsv $v01, 2, 38, $zero
  ori $s3, $zero, %lo(RSPQ_DMEM_BUFFER)
  ldv $v02, 0, 24, $zero
  ldv $v02, 8, 24, $zero
  ldv $v03, 0, 32, $zero
  ldv $v03, 8, 32, $zero
  ldv $v04, 0, 40, $zero
  ldv $v04, 8, 40, $zero
  ori $a1, $zero, %lo(TRI_BUFF)
  addiu $a2, $a1, 64
  LABEL_0001:
  mfc0 $ra, COP0_DMA_BUSY
  bne $ra, $zero, LABEL_0001
  addiu $a3, $a2, 64
  LABEL_0003:
  lpv $v05, 0, 0, $t0
  vmulf $v29, $v02, $v05.h0
  vmacf $v29, $v03, $v05.h1
  vmacf $v05, $v04, $v05.h2
  vsubc $v06, $v00, $v05.e6
  vmulu $v07, $v05, $v05.e6
  vmudh $v07, $v07, $v30.e5
  vmulu $v06, $v06, $v06.v
  vmulu $v06, $v06, $v06.v
  vmulu $v06, $v06, $v06.v
  vmulu $v08, $v06, $v06.v
  vmudh $v08, $v08, $v30.e6
  vmov $v08.e3, $v07.e6
  vmudm $v09, $v05, $v31.e5
  vaddc $v05, $v09, $v01.v
  sdv $v05, 0, 0, $a1 ## Barrier: 0x2
  suv $v08, 0, 8, $a1 ## Barrier: 0x2
  slv $v09, 8, 12, $a1 ## Barrier: 0x2
  addiu $t0, $t0, 8
  bne $t0, $t2, LABEL_0003
  addiu $a1, $a1, 64
  LABEL_0004:
  ori $a1, $zero, %lo(TRI_BUFF)
  LABEL_0005:
  mfc0 $t3, COP0_DP_CURRENT
  mfc0 $t4, COP0_DP_END
  bne $t3, $t4, LABEL_0005
  addiu $v0, $zero, 1
  LABEL_0006:
  mtc0 $s3, COP0_DP_START
  #define zero $0
  #define v0 $2
  #define v1 $3
  #define a0 $4
  #define a1 $5
  #define a2 $6
  #define a3 $7
  #define t0 $8
  #define t1 $9
  #define t2 $10
  #define t3 $11
  #define t4 $12
  #define t5 $13
  #define t6 $14
  #define t7 $15
  #define s0 $16
  #define s1 $17
  #define s2 $18
  #define s3 $19
  #define s4 $20
  #define s5 $21
  #define s6 $22
  #define s7 $23
  #define t8 $24
  #define t9 $25
  #define k0 $26
  #define k1 $27
  #define gp $28
  #define sp $29
  #define fp $30
  #define ra $31
  .set at
  .set macro
  #include "./rspq_triangle.inc"
  .set noreorder
  .set noat
  .set nomacro
  #undef zero
  #undef at
  #undef v0
  #undef v1
  #undef a0
  #undef a1
  #undef a2
  #undef a3
  #undef t0
  #undef t1
  #undef t2
  #undef t3
  #undef t4
  #undef t5
  #undef t6
  #undef t7
  #undef s0
  #undef s1
  #undef s2
  #undef s3
  #undef s4
  #undef s5
  #undef s6
  #undef s7
  #undef t8
  #undef t9
  #undef k0
  #undef k1
  #undef gp
  #undef sp
  #undef fp
  #undef ra
  RDPQ_Triangle_Skip:
  bne $s6, $s5, LABEL_0007
  addiu $t5, $zero, 41
  sb $t5, ($s3)
  addiu $s3, $s3, 8
  LABEL_0007:
  bne $s6, $s5, MAIN_LOOP
  mtc0 $s3, COP0_DP_END
  break # inline-ASM

OVERLAY_CODE_END:

#define zero $0
#define v0 $2
#define v1 $3
#define a0 $4
#define a1 $5
#define a2 $6
#define a3 $7
#define t0 $8
#define t1 $9
#define t2 $10
#define t3 $11
#define t4 $12
#define t5 $13
#define t6 $14
#define t7 $15
#define s0 $16
#define s1 $17
#define s2 $18
#define s3 $19
#define s4 $20
#define s5 $21
#define s6 $22
#define s7 $23
#define t8 $24
#define t9 $25
#define k0 $26
#define k1 $27
#define gp $28
#define sp $29
#define fp $30
#define ra $31

.set at
.set macro