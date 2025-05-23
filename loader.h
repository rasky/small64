#ifndef BOOT_LOADER_H
#define BOOT_LOADER_H

// The loader is run directly from RDRAM for speed. We reserve a portion at
// the end of RDRAM for it.
#define LOADER_SIZE              (28*1024)

// We put the first stage stack into the cache, at the same address as the loader.
// We reserve 4 KiB to it.
#define STACK1_SIZE              (4*1024)

#define TOTAL_RESERVED_SIZE      (LOADER_SIZE + STACK1_SIZE)

#define STACK1_BASE              (0x80800000 - LOADER_SIZE - STACK1_SIZE)
#define STACK1_TOP               (0x80800000 - LOADER_SIZE)

#define LOADER_BASE(memsize, stage2size)              (0x80000000 + (memsize) - (stage2size))
#define STACK2_TOP(memsize, stage2size)               (LOADER_BASE(memsize, stage2size) - 16)

#define LOADER_VADDR             0xE0000000

#ifndef __ASSEMBLER__
__attribute__((noreturn, far))
void demo(void);
#endif

#endif /* BOOT_LOADER_H */
