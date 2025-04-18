#ifndef BOOT_MINIDRAGON_H
#define BOOT_MINIDRAGON_H

#include <stdint.h>

typedef uint64_t u_uint64_t __attribute__((aligned(1)));

#if PROD
#define IPL3_ROMBASE   0xB0000000
#else
#define IPL3_ROMBASE   0xB0001000
#endif

#define MEMORY_BARRIER()            asm volatile ("" : : : "memory")
#define C0_WRITE_CAUSE(x)           asm volatile("mtc0 %0,$13"::"r"(x))
#define C0_WRITE_COUNT(x)           asm volatile("mtc0 %0,$9"::"r"(x))
#define C0_WRITE_COMPARE(x)         asm volatile("mtc0 %0,$11"::"r"(x))
#define C0_WRITE_WATCHLO(x)         asm volatile("mtc0 %0,$18"::"r"(x))
#define C0_WRITE_ENTRYLO0(x)        asm volatile("mtc0 %0,$2"::"r"(x))
#define C0_WRITE_ENTRYLO1(x)        asm volatile("mtc0 %0,$3"::"r"(x))
#define C0_WRITE_ENTRYLO1_ZERO()    asm volatile("mtc0 $0,$3")
#define C0_WRITE_PAGEMASK(x)        asm volatile("mtc0 %0,$5"::"r"(x))
#define C0_WRITE_ENTRYHI(x)         asm volatile("mtc0 %0,$10"::"r"(x))
#define C0_WRITE_INDEX(x)           asm volatile("mtc0 %0,$0"::"r"(x))
#define C0_WRITE_INDEX_ZERO()       asm volatile("mtc0 $0,$0")
#define C0_TLBWI()                  asm volatile("tlbwi")

#define C0_COUNT() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$9":"=r"(x)); \
    x; \
})

#define C0_TAGLO() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$28":"=r"(x)); \
    x; \
})

#define C0_STATUS() ({ \
    uint32_t x; \
    asm volatile("mfc0 %0,$12":"=r"(x)); \
    x; \
})
#define C0_WRITE_STATUS(x) ({ \
    asm volatile("mtc0 %0,$12"::"r"(x)); \
})

#define C0_STATUS_IE        0x00000001      ///< Status: interrupt enable
#define C0_STATUS_EXL       0x00000002      ///< Status: within exception 
#define C0_STATUS_ERL       0x00000004      ///< Status: within error

#define TICKS_TO_US(val) (((val) * 8 / (8 * 93750000/2 / 1000000)))

#ifdef DEBUG
#include <stdio.h>
#include <assert.h>
#define debugf(s, ...)   fprintf(stderr, s, ##__VA_ARGS__)
#define assertf(x, ...)  assert(x)
#else
#define assertf(x, ...)  ({ })
#define assert(x)        ({ })
#define debugf(s, ...)   ({ })
#endif

#define BIT(x, n)           (((x) >> (n)) & 1)
#define BITS(x, b, e)       (((x) >> (b)) & ((1 << ((e)-(b)+1))-1))

#define abs(x)             __builtin_abs(x)
#define fabsf(x)           __builtin_fabsf(x)

#define PI_STATUS          ((volatile uint32_t*)0xA4600010)
#define PI_STATUS_DMA_BUSY ( 1 << 0 )
#define PI_STATUS_IO_BUSY  ( 1 << 1 )

#define SI_STATUS          ((volatile uint32_t*)0xA4800018)
#define SI_STATUS_DMA_BUSY ( 1 << 0 )
#define SI_STATUS_IO_BUSY  ( 1 << 1 )

#define CACHE_I		            0
#define CACHE_D		            1
#define INDEX_INVALIDATE		0
#define INDEX_LOAD_TAG			1
#define INDEX_STORE_TAG			2
#define INDEX_CREATE_DIRTY      3
#define BUILD_CACHE_OP(o,c)		(((o) << 2) | (c))
#define INDEX_WRITEBACK_INVALIDATE_D	BUILD_CACHE_OP(INDEX_INVALIDATE,CACHE_D)
#define INDEX_STORE_TAG_I              	BUILD_CACHE_OP(INDEX_STORE_TAG,CACHE_I)
#define INDEX_STORE_TAG_D               BUILD_CACHE_OP(INDEX_STORE_TAG,CACHE_D)
#define INDEX_LOAD_TAG_I              	BUILD_CACHE_OP(INDEX_LOAD_TAG,CACHE_I)
#define INDEX_LOAD_TAG_D                BUILD_CACHE_OP(INDEX_LOAD_TAG,CACHE_D)
#define INDEX_CREATE_DIRTY_D            BUILD_CACHE_OP(INDEX_CREATE_DIRTY,CACHE_D)


#define SP_RSP_ADDR     ((volatile uint32_t*)0xa4040000)
#define SP_DRAM_ADDR    ((volatile uint32_t*)0xa4040004)
#define SP_RD_LEN       ((volatile uint32_t*)0xa4040008)
#define SP_WR_LEN       ((volatile uint32_t*)0xa404000c)
#define SP_STATUS       ((volatile uint32_t*)0xa4040010)
#define SP_DMA_FULL     ((volatile uint32_t*)0xa4040014)
#define SP_DMA_BUSY     ((volatile uint32_t*)0xa4040018)
#define SP_SEMAPHORE    ((volatile uint32_t*)0xa404001c)
#define SP_PC           ((volatile uint32_t*)0xa4080000)
#define SP_DMEM         ((uint32_t*)0xa4000000)
#define SP_IMEM         ((uint32_t*)0xa4001000)

#define SP_WSTATUS_CLEAR_HALT        0x00001   ///< SP_STATUS write mask: clear #SP_STATUS_HALTED bit
#define SP_WSTATUS_SET_HALT          0x00002   ///< SP_STATUS write mask: set #SP_STATUS_HALTED bit
#define SP_WSTATUS_CLEAR_BROKE       0x00004   ///< SP_STATUS write mask: clear BROKE bit
#define SP_WSTATUS_CLEAR_INTR        0x00008   ///< SP_STATUS write mask: clear INTR bit
#define SP_WSTATUS_SET_INTR          0x00010   ///< SP_STATUS write mask: set HALT bit
#define SP_WSTATUS_CLEAR_SSTEP       0x00020   ///< SP_STATUS write mask: clear SSTEP bit
#define SP_WSTATUS_SET_SSTEP         0x00040   ///< SP_STATUS write mask: set SSTEP bit
#define SP_WSTATUS_CLEAR_INTR_BREAK  0x00080   ///< SP_STATUS write mask: clear #SP_STATUS_INTERRUPT_ON_BREAK bit
#define SP_WSTATUS_SET_INTR_BREAK    0x00100   ///< SP_STATUS write mask: set SSTEP bit
#define SP_WSTATUS_CLEAR_SIG0        0x00200   ///< SP_STATUS write mask: clear SIG0 bit
#define SP_WSTATUS_SET_SIG0          0x00400   ///< SP_STATUS write mask: set SIG0 bit
#define SP_WSTATUS_CLEAR_SIG1        0x00800   ///< SP_STATUS write mask: clear SIG1 bit
#define SP_WSTATUS_SET_SIG1          0x01000   ///< SP_STATUS write mask: set SIG1 bit
#define SP_WSTATUS_CLEAR_SIG2        0x02000   ///< SP_STATUS write mask: clear SIG2 bit
#define SP_WSTATUS_SET_SIG2          0x04000   ///< SP_STATUS write mask: set SIG2 bit
#define SP_WSTATUS_CLEAR_SIG3        0x08000   ///< SP_STATUS write mask: clear SIG3 bit
#define SP_WSTATUS_SET_SIG3          0x10000   ///< SP_STATUS write mask: set SIG3 bit
#define SP_WSTATUS_CLEAR_SIG4        0x20000   ///< SP_STATUS write mask: clear SIG4 bit
#define SP_WSTATUS_SET_SIG4          0x40000   ///< SP_STATUS write mask: set SIG4 bit
#define SP_WSTATUS_CLEAR_SIG5        0x80000   ///< SP_STATUS write mask: clear SIG5 bit
#define SP_WSTATUS_SET_SIG5          0x100000  ///< SP_STATUS write mask: set SIG5 bit
#define SP_WSTATUS_CLEAR_SIG6        0x200000  ///< SP_STATUS write mask: clear SIG6 bit
#define SP_WSTATUS_SET_SIG6          0x400000  ///< SP_STATUS write mask: set SIG6 bit
#define SP_WSTATUS_CLEAR_SIG7        0x800000  ///< SP_STATUS write mask: clear SIG7 bit
#define SP_WSTATUS_SET_SIG7          0x1000000 ///< SP_STATUS write mask: set SIG7 bit

#define SP_STATUS_HALTED        (1<<0)
#define SP_STATUS_BROKE         (1<<1)
#define SP_STATUS_DMA_BUSY      (1<<2)
#define SP_STATUS_DMA_FULL      (1<<3)
#define SP_STATUS_IO_FULL       (1<<4)
#define SP_STATUS_SSTEP         (1<<5)
#define SP_STATUS_INTR_BREAK    (1<<6)
#define SP_STATUS_SIG0          (1<<7)
#define SP_STATUS_SIG1          (1<<8)
#define SP_STATUS_SIG2          (1<<9)
#define SP_STATUS_SIG3          (1<<10)
#define SP_STATUS_SIG4          (1<<11)
#define SP_STATUS_SIG5          (1<<12)
#define SP_STATUS_SIG6          (1<<13)
#define SP_STATUS_SIG7          (1<<14)

#define PI_DRAM_ADDR    ((volatile uint32_t*)0xA4600000)  ///< PI DMA: DRAM address register
#define PI_CART_ADDR    ((volatile uint32_t*)0xA4600004)  ///< PI DMA: cartridge address register
#define PI_RD_LEN       ((volatile uint32_t*)0xA4600008)  ///< PI DMA: read length register
#define PI_WR_LEN       ((volatile uint32_t*)0xA460000C)  ///< PI DMA: write length register
#define PI_STATUS       ((volatile uint32_t*)0xA4600010)  ///< PI: status register

#define MI_MODE                             ((volatile uint32_t*)0xA4300000)
#define MI_WMODE_CLEAR_REPEAT_MOD           0x80
#define MI_WMODE_SET_REPEAT_MODE            0x100
#define MI_WMODE_REPEAT_LENGTH(n)           ((n)-1)
#define MI_WMODE_SET_UPPER_MODE             0x2000
#define MI_WMODE_CLEAR_UPPER_MODE           0x1000
#define MI_VERSION                          ((volatile uint32_t*)0xA4300004)
#define MI_INTERRUPT                        ((volatile uint32_t*)0xA4300008)
#define MI_INTERRUPT_SP                     0x0001
#define MI_INTERRUPT_SI                     0x0002
#define MI_INTERRUPT_AI                     0x0004
#define MI_INTERRUPT_VI                     0x0008
#define MI_INTERRUPT_PI                     0x0010
#define MI_INTERRUPT_DP                     0x0020
#define MI_WINTERRUPT_CLR_SP                0x0001
#define MI_WINTERRUPT_SET_SP                0x0002
#define MI_WINTERRUPT_CLR_SI                0x0004
#define MI_WINTERRUPT_SET_SI                0x0008
#define MI_WINTERRUPT_CLR_AI                0x0010
#define MI_WINTERRUPT_SET_AI                0x0020
#define MI_WINTERRUPT_CLR_VI                0x0040
#define MI_WINTERRUPT_SET_VI                0x0080
#define MI_WINTERRUPT_CLR_PI                0x0100
#define MI_WINTERRUPT_SET_PI                0x0200
#define MI_WINTERRUPT_CLR_DP                0x0400
#define MI_WINTERRUPT_SET_DP                0x0800

#define AI_DRAM_ADDR                        ((volatile uint32_t*)0xA4500000)
#define AI_LENGTH                           ((volatile uint32_t*)0xA4500004)
#define AI_CONTROL                          ((volatile uint32_t*)0xA4500008)
#define AI_STATUS                           ((volatile uint32_t*)0xA450000C)
#define AI_DACRATE                          ((volatile uint32_t*)0xA4500010)
#define AI_BITRATE                          ((volatile uint32_t*)0xA4500014)

#define PI_CLEAR_INTERRUPT                  0x02
#define SI_CLEAR_INTERRUPT                  0
#define SP_CLEAR_INTERRUPT                  0x08
#define DP_CLEAR_INTERRUPT                  0x0800
#define AI_CLEAR_INTERRUPT                  0

#define VI_CTRL                             ((volatile uint32_t*)0xA4400000)
#define VI_ORIGIN                           ((volatile uint32_t*)0xA4400004)
#define VI_WIDTH                            ((volatile uint32_t*)0xA4400008)
#define VI_V_INTR                           ((volatile uint32_t*)0xA440000C)
#define VI_V_CURRENT                        ((volatile uint32_t*)0xA4400010)
#define VI_BURST                            ((volatile uint32_t*)0xA4400014)
#define VI_V_TOTAL                          ((volatile uint32_t*)0xA4400018)
#define VI_H_TOTAL                          ((volatile uint32_t*)0xA440001C)
#define VI_H_TOTAL_LEAP                     ((volatile uint32_t*)0xA4400020)
#define VI_H_VIDEO                          ((volatile uint32_t*)0xA4400024)
#define VI_V_VIDEO                          ((volatile uint32_t*)0xA4400028)
#define VI_V_BURST                          ((volatile uint32_t*)0xA440002C)
#define VI_X_SCALE                          ((volatile uint32_t*)0xA4400030)
#define VI_Y_SCALE                          ((volatile uint32_t*)0xA4400034)

#define DP_START                            ((volatile uint32_t*)0xA4100000)
#define DP_END                              ((volatile uint32_t*)0xA4100004)
#define DP_CURRENT                          ((volatile uint32_t*)0xA4100008)
#define DP_STATUS                           ((volatile uint32_t*)0xA410000C)

#define DP_STATUS_XBUS                      0x0001
#define DP_STATUS_FREEZE                    0x0002
#define DP_STATUS_FLUSH                     0x0004
#define DP_STATUS_START_GCLK                0x0008
#define DP_STATUS_TMEM_BUSY                 0x0010
#define DP_STATUS_PIPE_BUSY                 0x0020
#define DP_STATUS_CMD_BUSY                  0x0040
#define DP_STATUS_CBUF_READY                0x0080
#define DP_STATUS_DMA_BUSY                  0x0100
#define DP_STATUS_END_PENDING               0x0200
#define DP_STATUS_START_PENDING             0x0400

#define DP_WSTATUS_CLR_XBUS                 0x0001
#define DP_WSTATUS_SET_XBUS                 0x0002
#define DP_WSTATUS_CLR_FREEZE               0x0004
#define DP_WSTATUS_SET_FREEZE               0x0008
#define DP_WSTATUS_CLR_FLUSH                0x0010
#define DP_WSTATUS_SET_FLUSH                0x0020
#define DP_WSTATUS_CLR_TMEM_BUSY            0x0040
#define DP_WSTATUS_CLR_PIPE_BUSY            0x0080
#define DP_WSTATUS_CLR_BUFFER_BUSY          0x0100
#define DP_WSTATUS_CLR_CLOCK                0x0200

#define RI_MODE                             ((volatile uint32_t*)0xA4700000)
#define RI_CONFIG                           ((volatile uint32_t*)0xA4700004)
#define RI_CURRENT_LOAD                     ((volatile uint32_t*)0xA4700008)
#define RI_SELECT                           ((volatile uint32_t*)0xA470000C)
#define RI_REFRESH                          ((volatile uint32_t*)0xA4700010)

#define RI_CONFIG_AUTO_CALIBRATION          0x40
#define RI_SELECT_RX_TX                     0x14
#define RI_MODE_RESET                       0x0
#define RI_MODE_STANDARD                    (0x8|0x4|0x2)

#define RI_REFRESH_CLEANDELAY(x)            ((x) & 0xFF)
#define RI_REFRESH_DIRTYDELAY(x)            (((x) & 0xFF) << 8)
#define RI_REFRESH_AUTO                     (1<<17)
#define RI_REFRESH_OPTIMIZE                 (1<<18)
#define RI_REFRESH_MULTIBANK(x)             (((x) & 0xF) << 19)

#define RDRAM_REGS(chip)                    ((volatile uint32_t*)(0xA3F00000 + ((chip) << 10)))
#define RDRAM_REGS_BROADCAST                ((volatile uint32_t*)0xA3F80000)

#define RDRAM_REG_DEVICE_ID                 1
#define RDRAM_REG_DELAY                     2
#define RDRAM_REG_MODE                      3
#define RDRAM_REG_REF_ROW                   5
#define RDRAM_REG_RAS_INTERVAL              6

#define BITSWAP5(x)         ((BIT(x,0)<<4) | (BIT(x,1)<<3) | (BIT(x,2)<<2) | (BIT(x,3)<<1) | (BIT(x,4)<<0))
#define ROT16(x)            ((((x) & 0xFFFF0000) >> 16) | (((x) & 0xFFFF) << 16))

#define RDRAM_REG_MODE_DE       (1<<25)
#define RDRAM_REG_MODE_AS       (1<<26)
#define RDRAM_REG_MODE_CC(cc)   ((BIT((cc)^0x3F, 0) << 6)  | \
                                (BIT((cc)^0x3F, 1) << 14) | \
                                (BIT((cc)^0x3F, 2) << 22) | \
                                (BIT((cc)^0x3F, 3) << 7)  | \
                                (BIT((cc)^0x3F, 4) << 15) | \
                                (BIT((cc)^0x3F, 5) << 23))

#define RDRAM_REG_DEVICE_ID_MAKE(chip_id) \
    (BITS(chip_id, 0, 5) << 26 | \
     BITS(chip_id, 6, 6) << 23 | \
     BITS(chip_id, 7, 14) << 8 | \
     BITS(chip_id, 15, 15) << 7)


#define RDRAM_REG_DELAY_MAKE(AckWinDelay, ReadDelay, AckDelay, WriteDelay) \
   ((((AckWinDelay) & 7) << 3 << 24) | \
    (((ReadDelay)   & 7) << 3 << 16) | \
    (((AckDelay)    & 3) << 3 <<  8) | \
    (((WriteDelay)  & 7) << 3 <<  0))

#define RDRAM_REG_RASINTERVAL_MAKE(row_precharge, row_sense, row_imp_restore, row_exp_restore) \
    ((BITSWAP5(row_precharge) << 24) | (BITSWAP5(row_sense) << 16) | (BITSWAP5(row_imp_restore) << 8) | (BITSWAP5(row_exp_restore) << 0))


#define UncachedAddr(x)                     ((void*)((uint32_t)(x) | 0x20000000))

#define cache_op(addr, op, linesize, length) ({ \
    { \
        void *cur = (void*)((unsigned long)addr & ~(linesize-1)); \
        int count = (int)length + (addr-cur); \
        for (int i = 0; i < count; i += linesize) \
            asm ("\tcache %0,(%1)\n"::"i" (op), "r" (cur+i)); \
    } \
})

/** @brief Round n up to the next multiple of d */
#define ROUND_UP(n, d) ({ \
	typeof(n) _n = n; typeof(d) _d = d; \
	(((_n) + (_d) - 1) / (_d) * (_d)); \
})

#define cache_op2(addr, op, linesize, length) ({ \
    void *end = (void*)(ROUND_UP(((uint32_t)addr) + length - 1, linesize)); \
    do { \
        asm ("\tcache %0,(%1)\n"::"i" (op), "r" (addr)); \
        addr += linesize; \
    } while (addr < end); \
})

static inline void data_cache_hit_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(addr, 0x11, 16, length);
}   

static inline void data_cache_hit_writeback_invalidate(volatile void * addr, unsigned long length)
{
    cache_op(addr, 0x15, 16, length);
}   

static inline void data_cache_writeback_invalidate_all(void)
{
    cache_op((void*)0x80000000, INDEX_WRITEBACK_INVALIDATE_D, 0x10, 0x2000);
}

static inline void cop0_clear_cache(void)
{
    asm("mtc0 $0, $28");  // TagLo
    asm("mtc0 $0, $29");  // TagHi
    void *addr = (void*)0x80000000;
    void *end  = addr + 0x4000;
    do {
        asm ("\tcache %0,(%1)\n"::"i" (INDEX_STORE_TAG_D), "r" (addr));
        asm ("\tcache %0,(%1)\n"::"i" (INDEX_STORE_TAG_I), "r" (addr));
        addr += 16;
    } while (addr < end);
}

__attribute__((noreturn))
static inline void abort(void)
{
    while(1) {}
}

static inline uint32_t io_read(uint32_t vaddrx)
{
    while (*PI_STATUS & (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY)) {}
    volatile uint32_t *vaddr = (uint32_t *)vaddrx;
    return *vaddr;
}

static inline void wait(int n) {
    while (n--) asm volatile("");
}

static inline void si_write(uint32_t offset, uint32_t value)
{
    volatile uint32_t *vaddr = (volatile uint32_t *)(0xBFC00000 + offset);
    *vaddr = value;
}

static inline void si_wait(void)
{
    while (*SI_STATUS & (SI_STATUS_DMA_BUSY | SI_STATUS_IO_BUSY)) {}
    *SI_STATUS = SI_CLEAR_INTERRUPT;
}

// Missing from libgcc
static inline uint32_t __byteswap32(uint32_t x)  
{
    uint32_t y = (x >> 24) & 0xff;
    y |= (x >> 8) & 0xff00;
    y |= (x << 8) & 0xff0000;
    y |= (x << 24) & 0xff000000;
    return y;
}

static inline void rsp_dma_to_rdram(const void* dmem, void *rdram, int size)
{
    while (*SP_DMA_FULL) {}
    *SP_RSP_ADDR = (uint32_t)dmem;
    *SP_DRAM_ADDR = (uint32_t)rdram;
    *SP_WR_LEN = size-1;
    while (*SP_DMA_BUSY) {}
}

static inline void rsp_dma_from_rdram(void* dmem, const void *rdram, int size)
{
    while (*SP_DMA_FULL) {}
    *SP_RSP_ADDR = (uint32_t)dmem;
    *SP_DRAM_ADDR = (uint32_t)rdram;
    *SP_RD_LEN = size-1;
    while (*SP_DMA_BUSY) {}
}

#define byteswap32(x) (__builtin_constant_p(x) ? __builtin_bswap32(x) : __byteswap32(x))

#endif
