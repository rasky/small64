# Small64 - The first 4k on Nintendo 64

Submitted at Revision 2025.

This document gives technical insights on how Small64 works and how we
achieved it.

## Quick recap of N64 hardware

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

## How a standard N64 ROM works

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
with the title, the region, etc. Most of it is just conventional data though,
not really needed at runtime.

## How small64 works

To actually make a 4 KiB intro, we only have one option:

           ----------------------
            Small64 (4096 bytes)
           ----------------------

So our intro *has to be the header and the IPL3*. This means that the intro
must also take care of initializing RDRAM. And moreover, **the intro itself has to
match the hardcoded checksum that IPL1/2 is going to calculate**, otherwise
it will not boot.

Inserting some code in the header is a common technique on PC too, so we just
adapted the same logic to Nintendo 64. In our case, there is only one part
that we need to preserve: byte 2, 3, and 4 that are used by IPL1/2 to configure
the ROM access speed. Everything else can be used. As a nice touch, we also
store the ASCII name at offset 0x20 as expected in ROMs, so that it is
displayed correctly by flashcart menus and ROM managers.

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

It seems that because of this, the intro fails to boot on cold console; if
that's the case for you, just run another ROM for a few seconds and then try
again. It should then work. We're hopefully going to fix this in a followup
release.

## Compression

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

## Small64 boot process

Let's now see how the ROM is actually laid out:

           ---------------------------------
              Stage 0 ROM (0x0000 - 0x003F)
           ---------------------------------
               Stage 0 (0x0040 - 0x0168)
           ---------------------------------
           Compressed RDRAM init (0x168 - ~0x232)
           ---------------------------------
            Compressed Intro (~0x232 - ~0xFFC)
           ---------------------------------
           IPL2 hash matching cookie (0xFFC-0xFFF)
           ---------------------------------

So how do you decompress an intro if RAM is not available? That's what we do:

 * IPL2 loads what it believes to be the "IPL3" (offsets 0x40-0x1000) into 
   DMEM (RSP static RAM for data).
 * Stage 0 is where execution starts. It's offset 0x40 in the ROM, which is
   where the IPL3 entrypoint is.
 * Stage 0 contains the upkr decompression code and the payload for the next
   stages. It decompresses Stage 1 (RDRAM init) into IMEM. Notice that part of
   Stage 0 code is put in what is normally the header space (0x0-0x3F), and
   since that part isn't loaded into DMEM by IPL2, it is run directly from ROM.
 * After decompression, it jumps to IMEM to run Stage 1.
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
byte of the intro at offset 0x232, meaning that the intro itself has to fit
into 3534 (compressed) bytes.

## GPU hash cracking for an intro?

We reserved the last 4 bytes of the ROM for the bruteforcing cookie. What is it?

As explained above, IPL1/2 will verify the IPL3 checksum using a bespoke algorithm,
to make sure it matches a hardcoded value. If this check fails, the ROM won't boot,
so we need to make sure our final ROM matches this checksum. How can it be possible?

We perform GPU hash cracking (technically, a "pre-image attack") by tweaking the
last 8 bytes of the ROM testing millions and millions of values
until we find one that matches the requested checksum. This is a process
that can take multiple hours on modern GPUs (eg: ~18/24 hours on a Apple M1 Pro).
This technique is also used by Libdragon to release their own open-source
IPL3s, so [tooling for this](https://github.com/Polprzewodnikowy/ipl3hasher-new) was already available.

To perform the cracking we use two sets of free bits (called respectively 
X bits and Y bits by the tool). The X bits must be the last 32 bits of the ROM,
so there's not much to do (as explained, we reserve them). The Y bits instead
can be everywhere in the ROM; the tool supports specifying up to 32 bits for Y
bits, though most signing can succeed with only 20 of them. Since our ROM is
pretty full, we use another tool we wrote ([mips_free_bits.py](https://github.com/rasky/small64/blob/main/tools/mips_free_bits.py))
to search for empty bits in stage0. In fact, many MIPS opcodes don't really use
all of the 32 bits that made the opcode, but leave a few them undefined. The
VR4300 CPU luckily just ignores those, so we can use them as our Y bits for
the tool.


## Music

The initial experiments started with [dollchan bytebeat
tool](https://dollchan.net/bytebeat/), but it is essentially JavaScript,
which uses double floats internally. The hunch was that floats would be too slow
on N64 for rendering multichannel music in real time and it would be better to
use integers (but 64-bit ones!). However, doubles only have 52 bits in the
fraction, so converting the dollchan bytebeat to 64-bit integers on the N64
would likely be a headache. It would be better to compose the music using 64-bit
integers in the first place. Thus, new tools were needed.

The new tool was a simple VST2 instrument, written in Go, using the [vst2
library](https://github.com/pipelined/vst2). In the first iteration, it had:
  * All math was based on 64-bit integers. 
  * Two oscillators (sinusoidal, saw, triangle, square, and noise)
  * ADSR envelope, with linear slopes
  * Delay effect
  * One second-order filter per instrument (low/band/high/notch), with adjustable frequency and resonance
  * Pitch drop, with exponentially dropping frequency
  *  One global reverb (shared by all instruments), ported from [4klang/Sointu](https://github.com/vsariola/sointu)

Anyway, this was like WAAAY over the *speed* budget when tested on the emulator:
N64 wasn't able to even render a single instrument with the reverb in real time!
So, things had to be simplified a lot. After removing the reverb, delay, and
just keeping one oscillator per channel, the machine was still only fast enough
to render 4 channels. The song had 8 instruments, so we had to make every two
instruments share the same channel and ensure that in the composition, these
channels had no overlapping notes.

Next problem was the *size* budget. The first versions of the song were
consuming around 1.5k (after compressed) and given the inefficiencies of
compressing the MIPS instruction encodings, the visuals were going to need all
the bytes they could get. Thus, several further features had to be removed from
the synth.

In the end, the synth had:
  * 4 channels / 8 instruments, with every two instruments sharing a channel
  * Adjustable sustain-release envelope, with fixed length sustain per instrument
  * 1 oscillator (sinusoidal, triangle, noise)
  * Filter per instrument (high or band)

The song was composed in [MuLab](https://www.mutools.com/) with the note data
exported into a .mid file, and a quick converter to convert this to linear
arrays. There was no need to make patterns/order list kind of data storage,
because the LZ type decompression of UPKR already handled the repetitive
patterns very well. There were 4 tracks, with note numbers 1-127 representing
the triggering of instrument #1 of that channel (track), while note numbers
129-255 represented triggering of instrument #2 of that channel.

Finally, to get the instrument settings from the MuLab project file, the vst2
instrument was programmed to store its settings as JSON in the DAW project
(using the GetChunk/SetChunk mechanism). We were happy to see that MuLab did not
apply any compression to its project files, so it was relatively easy to write a
script to scrape the JSONs from the MuLab project file.


3D Graphics
=====

### Overview

Drawing 3D meshes on the N64 requires requires a good amount of code.<br>
While there is dedicated hardware in form of a rasterizer (the RDP), it will only process 2D screen-space triangles.<br>
On top of that, the format expects them to be pre-processed into starting points and slopes, instead of just a pair of 3 vertices (See [RDP Triangle](https://n64brew.dev/wiki/Reality_Display_Processor/Commands#0x08_through_0x0F_-_Fill_Triangle) for more information).<br>
This means that the entire 3D pipeline from transformation, lighting, clipping and slope calculations needs to be done in software.<br>

While it is possible to do this on the CPU, games will offload this onto the RSP with special code called "microcode" or "ucode".<br>

The main advantage here are the 32 vector registers with 8 lanes each (all 16bit integers).<br>
On top it can also execute a scalar and vector instruction at the same time under certain conditions.<br>

Usually the hardest part of writing ucode is understanding all the nuances of the rather special instruction set, as well as keeping it fast.<br>
Instructions can stall each other with complicated rules, requiring manual  re-ordering (See [RSP Pipeline](https://n64brew.dev/wiki/Reality_Signal_Processor/CPU_Pipeline)).<br>

Normally all of this is written directly in assembly, due to a lack of compiler support.<br>
There is however a high-level language called [RSPL](https://github.com/HailToDodongo/rspl) which was developed together with one of the homebrew 3D ucodes [Tiny3D](https://github.com/HailToDodongo/tiny3d), which this demo loosely used for reference.

### Demo Ucode

For this demo the ucode had to be written way differently than you usually would.<br>
Instead of running fully in parallel, it is instead synced with the CPU, which avoids having to implement a command queue system.<br>
The idea being that the RSP is halted by default, while the CPU can setup the data for the next task.<br>

Mesh data in form of unindexed triangles are generated once in RDRAM via the CPU.<br>
The exact location and size is hardcoded in the ucode.<br>

The other input parameters that change over time (rotation, scaling) are directly set to DMEM.<br>
Note that this can only be done here since the RSP is halted, otherwise it may cause bus conflicts.<br>

Each frame the CPU will calculate a fixed-point transformation matrix only containing rotation.<br>
Which intern can be used to rotate both the vertices and normals at the same time.<br>
Scaling is provided as a single scalar which gets applied after that.<br>

Once the RSP is started it will then load the parameters, and starts processing the triangles by streaming them in one by one.<br>
Each getting transformed, lighting and effects applied, and finally converted and send to the RDP for rasterization.<br>
Lastly it will stop itself halting the processor.<br>

In order to reduce instructions a lot of things where removed including: perspective, input data for UVs and color, clipping and rejection as well as some precision for the final RDP slopes.<br>

One of the challenges with that was to work with the compression in mind.<br>
For example the automatic reordering of RSPL was disabled for this demo.<br>
While it may not change the size, it created less "uniform" code.<br>
As a tradeoff this made the code run way slower than it could however.<br>

The process of how the RDP will fetch data was also changed.<br>
Normally you can point it to a buffer in RDRAM via a register from which to fetch commands.<br>
While this gives you a lot of memory to work with, it requires a DMA from the RSP to get it there.<br>
Instead we point it directly to DMEM to avoid that, with the tradeoff of dealing with the 4kb DMEM size and reduced performance once more due to more syncing.<br>

However only reducing code doesn't make for a great demo, so a few effects where squeezed in.<br>
All of which work by using the existing data with little extra code:

#### UV-Gen
UVs are generated based on the screen-space normals X and Y position.<br>
Which is a simplified form of spherical texture coordinates.<br>
The RDP will later draw a texture with that, where the texture data itself is simply random data in RDRAM.<br>
This gives the torus a metallic appearance.<br>

#### Fresnel
By taking the Z-component of the normals and scaling it, we can get a cheap fresnel factor.<br>
That factor is later used to blend towards another random texture for the colored outline effect.<br>

#### Specular
The specular highlights are faked by taking the previous fresnel factor and inverting it.<br>
Effectively simulating a light pointing directly at the screen.<br>
By multiplying it with itself a few times, which also compresses nicely, we can get a sharp highlight.<br>
This value is passed as vertex color to the RDP, and is then added on top of the texture color.<br>

#### Explosion Effect

A scalar can be passed in which which will displace a triangle.<br>
This will take the first vertex of a triangle together with parts of the matrix to generate an offset in screen-space.<br>
The exact calculation have no deeper meaning and where chosen randomly.<br>
By adding this scaled displacement onto all vertices of a triangle they will start flying across the screen.<br>
