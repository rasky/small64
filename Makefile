ifneq ($(V),1)
.SILENT:
endif

ifeq ($(OS),Windows_NT)
  EXE := .exe
else
  EXE :=
endif

SOURCE_DIR=.
COMPRESSION_LEVEL=9
SWIZZLE=1

# 0 = PAL, 1 = NTSC, 2 = MPAL
VIDEO_TYPE ?= 0

ifeq ($(VIDEO_TYPE),0)
ROM_NAME = small64_pal.z64
else ifeq ($(VIDEO_TYPE),1)
ROM_NAME = small64_ntsc.z64
else ifeq ($(VIDEO_TYPE),2)
ROM_NAME = small64_mpal.z64
else
$(error VIDEO_TYPE must be 0, 1 or 2)
endif


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
N64_NM = $(N64_GCCPREFIX_TRIPLET)nm
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
N64_CFLAGS += -ffreestanding -nostdlib -ffunction-sections -fdata-sections -fno-tree-loop-optimize
N64_CFLAGS += -G0 # gp is not initialized (don't use it)
#N64_CFLAGS += -mabi=32 -mgp32 -mfp32 -msingle-float # Can't compile for 64bit ABI because DMEM/IMEM don't support 64-bit access
N64_CFLAGS += -ffast-math -ftrapping-math -fno-associative-math
#N64_CFLAGS += -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=unused-but-set-variable -Wno-error=unused-value -Wno-error=unused-label -Wno-error=unused-parameter -Wno-error=unused-result
N64_CFLAGS += -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -Wno-unused-value -Wno-unused-label -Wno-unused-parameter -Wno-unused-result
#N64_CFLAGS += -ffast-math
#N64_CFLAGS += -flto

N64_ASFLAGS = -mtune=vr4300 -march=vr4300 -Wa,--fatal-warnings
#N64_ASFLAGS =  -mabi=32 -mgp32 -mfp32 -msingle-float -G0
N64_RSPASFLAGS = -march=mips1 -mabi=32 -Wa,--fatal-warnings
N64_LDFLAGS = -Wl,-Tsmall.1.ld -Wl,-Map=build/small.map -Wl,--gc-sections

SHRINKER ?= ../Shrinkler/build/native/Shrinkler

# Objects used for the first compilation step (uncompressed)
BOOT_OBJS = build/stage0.o
STAGE2_OBJS = build/demo.o build/torus.o # build/minilib.o

# Sources used to build the final compressed binary
FINAL_SRCS = stage0.S stage0_bins.S

N64_ASPPFLAGS += -DNDEBUG -DPROD
N64_CFLAGS += -DNDEBUG -DPROD -DVIDEO_TYPE=$(VIDEO_TYPE)

build/demo.o: N64_CFLAGS += -G1024

all: $(ROM_NAME)

build/%.o: %.c
	@echo "    [CC] $@"
	@mkdir -p build
	$(N64_CC) -c $(N64_CFLAGS) -Wa,--no-warn -o $@ $<;

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
	@mkdir -p build
	$(N64_CC) $(N64_RSPASFLAGS) -L$(N64_LIBDIR) -nostartfiles -Wl,-Trsp.ld -Wl,--gc-sections  -Wl,-Map=$(BUILD_DIR)/$(notdir $(basename $@)).map -o $@.elf $<
	$(N64_OBJCOPY) -O binary -j .text $@.elf $@.text.bin
	$(N64_OBJCOPY) -O binary -j .data $@.elf $@.data.bin
	$(N64_SIZE) -G $@.elf
	./tools/ucode_to_inc.py $@.data.bin $@.text.bin build/rsp_u3d.inc
	rm $@.elf $@.text.bin $@.data.bin

# Upkr tool
build/upkr$(EXE): tools/upkr/src/main.rs
	@echo "    [CARGO] $@"
	@mkdir -p build
	cd tools/upkr && cargo build --release --quiet
	cp tools/upkr/target/release/upkr$(EXE) $@

# Upkr library
build/libupkr.a:
	@echo "    [CARGO] $@"
	@mkdir -p build
	cd tools/upkr/c_library && cargo build --release --quiet
	cp tools/upkr/c_library/target/release/libupkr.a build/
	cp tools/upkr/c_library/upkr.h build/

# ipl3hasher tool
build/ipl3hasher-new$(EXE):
	@echo "    [CARGO] $@"
	@mkdir -p build
	cd tools/ipl3hasher-new && cargo build --release --quiet
	cp tools/ipl3hasher-new/target/release/ipl3hasher-new$(EXE) build/

# Swizzle tool
build/swizzle3: tools/swizzle3.cpp build/libupkr.a
	@echo "    [TOOL] $@"
	@mkdir -p build
	$(CXX) -O2 -std=c++20 -Itools -Ibuild -o $@ $^

# Swizzle the order of the sections in the final binary
build/order.ld: $(STAGE2_OBJS) build/swizzle3
	@echo "    [SWIZZLE] $@"
	@mkdir -p build
	if [ ${SWIZZLE} -eq 1 ]; then \
		./build/swizzle3 $@ $(BOOT_OBJS) $(STAGE2_OBJS); \
	else \
		./build/swizzle3 --quick $@ $(BOOT_OBJS) $(STAGE2_OBJS); \
	fi

# Build initial binary with all stages (uncompressed), using the optimized order
build/small.elf: small.1.ld build/order.ld $(BOOT_OBJS) $(STAGE2_OBJS) build/rsp_u3d.inc
	@echo "    [LD] $@"
	$(N64_CC) $(N64_CFLAGS) $(N64_LDFLAGS) -Wl,--entry=0 -o $@ $(filter %.o,$^)

# Extract and compress stages
build/stage12.bin: build/small.elf build/upkr$(EXE)
	@echo "    [SHRINK] $@"
	$(N64_OBJCOPY) -O binary -j .text.stage1 $< build/stage1.bin.raw
	$(N64_OBJCOPY) -O binary -j .text.stage2 $< build/stage2.bin.raw
	$(N64_OBJCOPY) -O binary -j .text.stage2u $< build/stage2u.bin.raw
	cat build/stage1.bin.raw build/stage2.bin.raw build/stage2u.bin.raw >$@.raw
	build/upkr$(EXE) -${COMPRESSION_LEVEL} --parity 4 $@.raw $@; \
	build/upkr$(EXE) --heatmap --parity 4 $@; \
	tools/heatmap.py --heatmap build/stage12.heatmap $< .text.stage1 .text.stage2 .text.stage2u | head -n 10; \

stats: build/stage12.bin
	tools/heatmap.py --heatmap build/stage12.heatmap build/small.elf .text.stage1 .text.stage2

heatmap: build/heatmap.html

build/heatmap.html: build/stage12.bin
	tools/heatmap.py --heatmap build/stage12.heatmap --html build/small.elf .text.stage1 .text.stage2 >$@
	open $@

# Build final binary with compressed stages
$(ROM_NAME): build/small.elf small.2.ld build/stage12.bin $(FINAL_SRCS)
	@echo "    [Z64] $@"
	$(N64_CC) $(N64_CFLAGS) -Wl,-Tsmall.2.ld -Wl,-Map=build/small.compressed.map \
		-DVIDEO_TYPE=$(VIDEO_TYPE) \
		-DSTAGE1_SIZE=$(strip $(shell wc -c < build/stage1.bin.raw)) \
		-DCOMPRESSION_ALGO=$(COMPRESSION_ALGO) \
		-o build/small.compressed.elf \
		$(FINAL_SRCS)
	$(N64_READELF) --wide --sections build/small.compressed.elf | grep .text
	$(N64_SIZE) -G build/small.compressed.elf
	$(N64_OBJCOPY) -O binary build/small.compressed.elf $@
	$(N64_OBJCOPY) -O binary -j .text.stage0 build/small.compressed.elf build/stage0.bin
	echo "$@ built successfully"

run: $(ROM_NAME)
	sc64deployer upload --direct $(ROM_NAME) && sc64deployer debug --isv 0x3FF0000

sign: $(ROM_NAME) build/ipl3hasher-new$(EXE)
	@echo "    [SIGN] $(ROM_NAME)"
	build/ipl3hasher-new$(EXE) --sign --cic 6102 --y-bits $$(tools/mips_free_bits.py --base 0x40 --skip 4 --limit 32 build/stage0.bin) --y-init 0 $(ROM_NAME)

disasm: build/small.elf
	$(N64_OBJDUMP) -D build/small.elf

clean:
	rm -rf build $(ROM_NAME) run

-include $(wildcard build/*.d)

.PHONY: all disasm run heatmap stats sign
