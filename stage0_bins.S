    .section .stage1bin, "ax"
    .balign 4
    .globl __stage1
__stage1:
    .incbin "build/stage1.bin"

    .section .stage2bin, "ax"
    .balign 4
    .globl __stage2
__stage2:
    .incbin "build/stage2.bin"
