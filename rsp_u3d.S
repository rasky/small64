## Auto-generated file, transpiled with RSPL
#define SCREEN_WIDTH_BYTES 1280
#define SCREEN_WIDTH_BYTES_DMA 1279
#define SCREEN_BYTES 307200
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
  STATE_MEM_START:
  SHIFT_DATA: .byte 128, 64, 32, 16, 8, 4, 2, 1
  .align 2
  VERTEX_ADDR: .ds.b 4
  .align 2
  DPL_ADDR: .ds.b 4
  STATE_MEM_END:

.bss
  TEMP_STATE_MEM_START:
    .align 4
    DPL_BUFF: .ds.b 1024
  TEMP_STATE_MEM_END:

.text
OVERLAY_CODE_START:

RSPQ_Loop:

Main:
  vxor $v00, $v00, $v00 # inline-ASM
  lpv $v31, 0, 0, $zero
  vmudl $v30, $v31, $v31.e7
  addiu $t0, $zero, 41
  addiu $t1, $zero, 54
  lw $t2, %lo(DPL_ADDR + 0)
  ori $t3, $zero, %lo(DPL_BUFF)
  ori $t4, $zero, %lo(DPL_BUFF)
  vxor $v01, $v00, $v30.e0
  vxor $v02, $v00, $v00.e0
  vmov $v02.e4, $v30.e3
  vmov $v02.e5, $v30.e3
  vaddc $v02, $v02, $v01.v
  mfc2 $t5, $v02.e1
  andi $t5, $t5, 4095
  mfc2 $t7, $v02.e0
  sll $t7, $t7, 12
  or $t5, $t5, $t7
  mfc2 $t6, $v02.e5
  andi $t6, $t6, 4095
  mfc2 $t7, $v02.e4
  sll $t7, $t7, 12
  or $t6, $t6, $t7
  sw $t5, 4($t3)
  sw $t6, 0($t3) ## Barrier: 0x1
  sb $t1, 0($t3) ## Barrier: 0x1
  addiu $t3, $t3, 8
  sb $t0, ($t3)
  addiu $t3, $t3, 8
  subu $t5, $t3, $t4
  mtc0 $t4, COP0_DMA_SPADDR ## Barrier: 0x2
  mtc0 $t2, COP0_DMA_RAMADDR ## Barrier: 0x2
  mtc0 $t5, COP0_DMA_WRITE ## Barrier: 0x2
  LABEL_0001:
  mfc0 $ra, COP0_DMA_BUSY
  bne $ra, $zero, LABEL_0001
  nop
  LABEL_0002:
  addu $t2, $t2, $t5
  mtc0 $t2, COP0_DP_END
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