# Small64 - The first 4k on Nintendo 64

Submitted at Revision 2025.

This document gives technical insights on how Small64 works and how we
achieved it.

Quick recap of N64 hardware
===========================
This is a short description of N64 hardware just to frame your mind about
the challenge:

 * MIPS R4300 64-bit CPU running at 93.750 Mhz, with FPU.
 * 4 MiB RDRAM, plus 4 MiB available through the expansion pak.
 * RSP 32-bit coprocessor, running at 62.500 Mhz, based on a custom 32-bit
   MIPS core plus custom SIMD extensions (128-bit registers with 8 16-bit
   fixed point slots). This is basically a DSP with internal 4K IMEM + 4K DMEM
   static memory for code and data, plus DMA access to RDRAM. This is where 3D
   transform and lighting normally runs.
 * RDP: Rasterizer doing screen-space triangle drawing with texture mapping,
   perspective correction, Z-buffering, etc. Not programmable.
 * Audio: just a plain DAC playing back 16-bit stereo samples from RDRAM.

How a standard N64 ROM works
============================
Normally, a N64 ROM has the following layout:

           ----------------------
              Header (64 bytes)
           ----------------------
              IPL3 (4032 bytes)
           ----------------------
                 Actual game



           ----------------------

When powering on the console, the CPU runs some bootstrap come burnt into
the silicon that is called IPL1/2. That code accesses the cartridge,
loads the IPL3 into some static memory, verifies that its checksum matches
a hardcoded value (that is stored in a security chip in the cartridge itself),
and then jumps to it.

IPL3 is stored in the game cartridge, but is actually a piece of bootcode that
was provided by Nintendo that is the last stage of the secure boot of the
console. IPL3 initializes the main RDRAM of the system, that involves also
a complex current calibration process, and then loads the actual game
(it assumes a 1 MiB flat binary) and jumps to it. This is where game developers
actually started providing their own code.

The 64-byte header of the ROM contains several metadata about the ROM, starting
with the title, the region, etc. It is interesting to notice that, even
though all ROMs, including homebrew, adhere to this layout, in practice
only very few bytes of the header are actually used at runtime: 
 * the ROM access speed (4 bytes), read by IPL1/2 to make sure it does access
   the cartridge at the correct speed supported by the ROM chips.
 * the game entrypoint (4 bytes) which is read by IPL3 to know where to jump
   when it's done.


How small64 works
=================
To actually make a 4 KiB intro, we only have one option:

           ----------------------
            Small64 (4096 bytes)
           ----------------------

So our intro *has to be the header and the IPL3*. This means that the intro
must also take care of initializing RDRAM. And moreover, the intro itself has to
match the hardcoded checksum that IPL1/2 is going to calculate, otherwise
it will not boot.

Inserting some code in the header is a common technique on PC too, so we just
adapted the same logic to Nintendo 64. In our case, there is only one part
that we need to preserve: byte 2, 3, and 4 that are used by IPL1/2 to configure
the ROM access speed. Everything else can be used. As a nice touch, we also
stored the ASCII name at offset 0x20, so that it is displayed correctly by
flashcart menus and ROM managers.

To boot the console, we had to write our own *compact* RDRAM initialization
routine. This was a bit challenging if you consider that the full initialization
process has been reverse engineered at the end of 2023 (!). Up until 2023, 
everybody still used a IPL3 payload ripped from commercial games to perform
the initialization. In 2023, Libdragon published the first unencumbered, open-source
implementation of IPL3, that's been in use in homebrew productions since.

Libdragon's code performs initialization as documented by Rambus datasheets,
so the process is a bit cumbersome and involves various steps including
output current calibration. For small64, we want for a totally barebone
approach, where we basically bang hardcoded values to various registers in
sequence. Current calibration is not performed: we only use a fixed value
that appears to work perfectly fine on most console at least when they are
semi-warm. In the end, our RAM init code is just 0x2ec bytes, before compression.

Compression
===========
For ROM compression, we needed something that could run on the MIPS CPU,
even *before* the RAM is initialized, as we wanted to also compress RAM init code.
Libdragon ships various algorithms including Shrinkler, which is the "grandfather"
of the famous Crinkler used by most intros on PC.

For small64, we want instead with upkr tool (https://github.com/exoticorn/upkr)
because it showed a slightly harder compression ratio, especially because it
allows for 4-byte parity for literal contexts, which gives a bit of advantage
on MIPS whose opcodes are 32-bit.

To improve upkr ratio, we also wrote our own custom section ordering tool called
"swizzle". This tool basically runs a simulated annealing optimizer to 
try different permutation of code and data sections (functions and data),
to find an order that maximizes the ratio. This technique is also used by
Crinkler but we didn't have time to write a full linker, so it basically
works with ELF .o files, and outputs a linker script (order.ld) containing
the correct order for the GCC linker to produce the final binary. 

Swizzle is able to save about 50 compressed bytes, which is quite a bit when
you fight for the byte!

The final payload of the intro is 164 KiB (or 37 KiB if you exclude the BSS
segment), which compresses down to 3786 bytes. Not bad! We include the BSS
segment in the compression because zeros compress very well, and so that the
decompression will clear that memory.

Small64 boot process
====================

Let's now see how the ROM is actually laid out:

           ---------------------------------
             Post-stage0 (0x0000 - 0x003F)
           ---------------------------------
               Stage 0 (0x0040 - 0x019B)
           ---------------------------------
           Compressed RDRAM init (0x19B - ~0x23B)
           ---------------------------------
            Compressed Intro (~0x23B - ~0xFF7)
           ---------------------------------
           IPL2 hash matching cookie (0xFF8-0xFFF)
           ---------------------------------

So how do you decompress an intro if RAM is not available? That's what we do:

 * IPL2 loads what it believes to be the "IPL3" (offsets 0x40-0x1000) into 
   DMEM (RSP static RAM for data).
 * Stage 0 is where execution starts (it's offset 0x40 in the ROM, which is
   where the IPL3 entrypoint is).
 * Stage 0 contains the upkr decompression code and the payload for the next
   stages. First it decompresses Stage 1 (RDRAM init) into the CPU data cache (!). 
   We use the data cache as it allows for 8-bit reads/writes that are needed for
   upkr to work correctly.
 * After decompression, it jumps to the Post-stage0 code in *ROM*. This is
   normally where the header is, but we put our small piece of code there.
   This code copies the decompressed Stage 1 from CPU data cache to IMEM.
   This is necessary because the CPU cannot execute code from the data cache.
   Then, it jumps to IMEM to to run it.
 * Now Stage 1 runs. This is the RDRAM init code. It initializes RDRAM so that
   we finally have our 4 MiB of RAM available for the intro. Then, it jumps
   back to Stage 0.
 * Stage 0 now runs decompression a second time. This time, it decompresses
   Stage 2 (compressed intro) to RDRAM. To be precise, it also decompresses
   again Stage 1 to RDRAM because Stage 1 and 2 are solidly compressed as 
   a single payload to improve ratio, so Stage 1 must be decompressed before
   Stage 2.
 * Then, Stage 0 jumps to the Stage 2 entrypoint in RDRAM. And now the
   actual intro begins!

Wow, quite a journey! All in all, we managed to have to first compressed
byte of the intro at offset 0x23B, meaning that the intro itself

The last 8 bytes were reserved for the bruteforcing cookie. If you remember,
IPL1/2 will verify the IPL3 checksum to make sure it matches a hardcoded
value. If this check fails, the ROM won't boot, so we need to make sure
our final ROM matches this checksum. How can it be possible?

To do so, we perform GPU hash cracking (technically, a "pre-image attack")
by tweaking the last 8 bytes of the ROM testing millions and millions of values
until we find one that matches the requested checksum. This is a process
that can take multiple hours on modern GPUs (eg: ~18/24 hours on a M1 Pro).
This technique is also used by Libdragon to release their own open-source
IPL3s, so tooling for this was already available.
