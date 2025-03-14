ifneq ($(V),1)
.SILENT:
endif

SOURCE_DIR=.
COMPRESSION_LEVEL=9

ifeq ($(shell uname -s),Darwin)
	SED=gsed
else
	SED=sed
endif

N64_GCCPREFIX ?= $(N64_INST)
N64_GCCPREFIX_TRIPLET = $(N64_GCCPREFIX)/bin/mips64-elf-
N64_CC = $(N64_GCCPREFIX_TRIPLET)gcc
N64_AS = $(N64_GCCPREFIX_TRIPLET)as
N64_LD = $(N64_GCCPREFIX_TRIPLET)ld
N64_OBJCOPY = $(N64_GCCPREFIX_TRIPLET)objcopy
N64_OBJDUMP = $(N64_GCCPREFIX_TRIPLET)objdump
N64_SIZE = $(N64_GCCPREFIX_TRIPLET)size
N64_READELF = $(N64_GCCPREFIX_TRIPLET)readelf

N64_ROOTDIR = $(N64_INST)
N64_INCLUDEDIR = $(N64_ROOTDIR)/mips64-elf/include
N64_LIBDIR = $(N64_ROOTDIR)/mips64-elf/lib
N64_MKASSET = $(N64_ROOTDIR)/bin/mkasset

N64_CFLAGS =  -march=vr4300 -mtune=vr4300 -MMD
N64_CFLAGS += -DN64 -Os -Wall -Wno-error=deprecated-declarations -fdiagnostics-color=always
N64_CFLAGS += -Wno-error=unused-function -ffreestanding -nostdlib -ffunction-sections -fdata-sections
N64_CFLAGS += -G0 # gp is not initialized (don't use it)
N64_CFLAGS += -mabi=32 -mgp32 -mfp32 -msingle-float # Can't compile for 64bit ABI because DMEM/IMEM don't support 64-bit access
N64_CFLAGS += -ffast-math -ftrapping-math -fno-associative-math
N64_CLFAGS += -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-value -Wno-error=unused-label -Wno-error=unused-parameter -Wno-error=unused-result
#N64_CFLAGS += -flto

N64_ASFLAGS = -mtune=vr4300 -march=vr4300 -Wa,--fatal-warnings
N64_ASFLAGS =  -mabi=32 -mgp32 -mfp32 -msingle-float -G0
N64_RSPASFLAGS = -march=mips1 -mabi=32 -Wa,--fatal-warnings
N64_LDFLAGS = -Wl,-Tsmall.1.ld -Wl,-Map=build/small.map -Wl,--gc-sections

SHRINKER ?= ../Shrinkler/build/native/Shrinkler

# Objects used for the first compilation step (uncompressed)
OBJS = build/stage1.o build/minidragon.o build/rdram.o
OBJS += build/demo.o build/minilib.o

# Sources used to build the final compressed binary
FINAL_SRCS = stage0.S stage0_bins.S

N64_ASPPFLAGS += -DNDEBUG -DPROD
N64_CFLAGS += -DNDEBUG -DPROD

build/demo.o: N64_CFLAGS += -G1024

all: small.z64

build/%.o: %.c
	@echo "    [CC] $@"
	@mkdir -p build
# Compile relocatable code. We need this for stage 1/2 which are relocated,
# but in general it won't hurt for boot code. GCC MIPS doesn't have a way to do
# this simple transformation so we do it ourselves by replacing jal/j with bal/b
# on the assembly output of the compiler.
	$(N64_CC) -S -c $(N64_CFLAGS) -o $@.s $<
	$(SED) -i 's/\bjal\b/bal/g' $@.s
	$(SED) -i 's/\bj\b/b/g' $@.s
	$(N64_AS) $(N64_ASFLAGS) --no-warn -o $@ $@.s

BUILD_DIR = build
SOURCE_DIR = .

build/%.o: %.S
	@echo "    [AS] $@"
	@mkdir -p build
	$(N64_CC) -c $(N64_ASFLAGS) $(N64_ASPPFLAGS) -o $@ $<; \

# RSP ucode
build/demo.o: build/rsp_u3d.inc
build/rsp_u3d.inc: rsp_u3d.S
	@echo "    [RSP] $<"
	$(N64_CC) $(N64_RSPASFLAGS) -L$(N64_LIBDIR) -nostartfiles -Wl,-Trsp.ld -Wl,--gc-sections  -Wl,-Map=$(BUILD_DIR)/$(notdir $(basename $@)).map -o $@.elf $<
	$(N64_OBJCOPY) -O binary -j .text $@.elf $@.text.bin
	$(N64_OBJCOPY) -O binary -j .data $@.elf $@.data.bin
	$(N64_SIZE) -G $@.elf
	xxd -n rsp_code -i $@.text.bin >$@
	xxd -n rsp_data -i $@.data.bin >>$@
	sed -i -e 's/unsigned /__attribute__((aligned(8))) const unsigned /g' $@
	rm $@.elf $@.text.bin $@.data.bin

# Build initial binary with all stages (uncompressed)
build/small.elf: small.1.ld $(OBJS) build/rsp_u3d.inc
	@echo "    [LD] $@"
	$(N64_CC) $(N64_CFLAGS) $(N64_LDFLAGS) -o $@ $(filter %.o,$^)

# Extract and compress stages
build/stage12.bin: build/small.elf
	@echo "    [SHRINK] $@"
	$(N64_OBJCOPY) -O binary -j .text.stage1 $< build/stage1.bin.raw
	$(N64_OBJCOPY) -O binary -j .text.stage2 $< build/stage2.bin.raw
	cat build/stage1.bin.raw build/stage2.bin.raw >$@.raw
	$(SHRINKER) -d -${COMPRESSION_LEVEL} -p $@.raw $@ >/dev/null

# Build final binary with compressed stages
%.z64: build/small.elf small.2.ld build/stage12.bin
	@echo "    [Z64] $@"
	$(N64_CC) $(N64_CFLAGS) -Wl,-Tsmall.2.ld -Wl,-Map=build/small.compressed.map \
		-DSTAGE1_SIZE=$(strip $(shell wc -c < build/stage1.bin.raw)) \
		-o build/small.compressed.elf \
		$(FINAL_SRCS)
	$(N64_READELF) --wide --sections build/small.compressed.elf | grep .text
	$(N64_SIZE) -G build/small.compressed.elf
	$(N64_OBJCOPY) -O binary build/small.compressed.elf $@

disasm: build/small.elf
	$(N64_OBJDUMP) -D build/small.elf

clean:
	rm -rf build small.z64

-include $(wildcard build/*.d)

.PHONY: all disasm run
