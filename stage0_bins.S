    .section .stage12bin, "ax"
    .balign 4
    .globl __stage12
__stage12:
    .incbin "build/stage12.bin"
