OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)
EXTERN (_start)
ENTRY (_start)

MEMORY
{
    dmem : ORIGIN = 0xA4000000, LENGTH = 4096
    imem : ORIGIN = 0xA4001000, LENGTH = 4096
    rom  : ORIGIN = 0xB0001000, LENGTH = 16384
    ram  : ORIGIN = 0x80000000, LENGTH = 256*1024
    uram  : ORIGIN = 0xA0000000, LENGTH = 256*1024
}

SECTIONS {
    /DISCARD/ : { *(.MIPS.abiflags) }
    /DISCARD/ : { *(.comment) }
    /DISCARD/ : { *(.reginfo) }
    /DISCARD/ : { *(.mdebug.*) }
    /DISCARD/ : { *(.gnu.*) }

    .text.prologue 0xA4000000 : {
        KEEP(*(.header))
    } > dmem

    .text.stage0 : {
        *(.stage0)
    } > dmem

    .text.banner : {
        *(.banner)
    } > dmem

    .text.stage1 : {
        __stage1 = .;
        KEEP(*(.stage1))
        build/stage1.o       
        build/rdram.o      
        . = ALIGN(8);
    } > imem

    .text.stage2 0x80000000 + SIZEOF(.text.stage1) : {
        _gp = . + 0x8000;
        __stage2 = .;
        __stage2_start = .;
        KEEP(build/demo.o(.text.demo))
        build/demo.o(.text .text.*)
        build/demo.o(.rodata .rodata.*)
        build/demo.o(.data .data.*)
        build/demo.o(.sdata .sdata.*)
        build/demo.o(.bss .bss.*)
        build/demo.o(.sbss .sbss.*)
        build/minilib.o
        . = ALIGN(8);
    } > ram

    .text.stage2u 0xA0000000 + SIZEOF(.text.stage1) + SIZEOF(.text.stage2) : {
        build/demo.o(.uncached)
    } > uram
}
