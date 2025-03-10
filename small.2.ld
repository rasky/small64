OUTPUT_FORMAT ("elf32-bigmips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH (mips)

MEMORY
{
    rom  : ORIGIN = 0xB0000000, LENGTH = 65536
    dmem : ORIGIN = 0xA4000000, LENGTH = 4096
}

SECTIONS {
    .text.header 0xB0000000 : {
        KEEP(*(.header))
    } > rom

    .text.stage0 0xA4000040 : AT ( 0xB0000040 ) {
        *(.stage0)
    } > dmem

    .text.stage1cmp : {
        *(.stage1bin)
    } > dmem

    .text.stage2cmp : {
        *(.stage2bin)
    } > dmem

    .text.signature 0xA4000FF8 : {
        QUAD(0x0000000000000000);
    } > dmem

    /DISCARD/ : { *(.text.boot_trampoline) }
    /DISCARD/ : { *(.text.ique_trampoline) }
    /DISCARD/ : { *(.MIPS.abiflags) }
}
