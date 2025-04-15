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

    .text.stage12cmp : {
        *(.stage12bin)
    } > dmem

    .text.signature 0xA4000FF9 : {
        /* IPL2 signature. Longest ever bruterforced for libdragon
           IPL3 was 7 bytes. */
        BYTE(0x00)
        SHORT(0x0000)
        LONG(0x00000000)
    } > dmem

    /DISCARD/ : { *(.text.boot_trampoline) }
    /DISCARD/ : { *(.text.ique_trampoline) }
    /DISCARD/ : { *(.MIPS.abiflags) }
}
