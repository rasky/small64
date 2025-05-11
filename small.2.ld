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

    .text.signature 0xA4000FFC : {
        /* IPL2 signature. We need to reserve 32-bit at the end of the ROM,
           plus we will use various unused bits in stage0 as found by 
           mips_free_bits.py */
        LONG(0x00000000)
    } > dmem

    /DISCARD/ : { *(.stage1) }
    /DISCARD/ : { *(.MIPS.abiflags) }
}
