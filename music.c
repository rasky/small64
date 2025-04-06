#include "minimath.h"
#include <stdint.h>
#define SONG_FREQUENCY 32000
#define MUSIC_BPM 144
#define MUSIC_CHANNELS 4

static const char bbsong_data[] = "80a50a80a50a805af0a50bf1a73cf3c7g0a80ag0a80ag08ag0a80ag0f137c3578db5db8da5dbfd5a7ce3ce7ce3ec875fg0a80ag0a80ag08ag0a80ag0f137c357";

typedef struct
{
    uint8_t attack, decay, sustain, release;
    uint8_t waveform, transpose, volume; // 0-3
    uint8_t filtType, filtRes, filtFreq;
    uint8_t pitchDrop; // 0-128, 0 no drop
} SynthParams;

static const uint8_t noteData[][580] = {
    {166, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 62, 0, 62, 0, 62, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 62, 0, 62, 0, 62, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 1, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 62, 0, 62, 0, 62, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 190, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 190, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 69, 0, 67, 0, 65, 0, 69, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 65, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 69, 0, 67, 0, 69, 0, 74, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 69, 0, 67, 0, 65, 0, 69, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 65, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 69, 0, 67, 0, 69, 0, 74, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 69, 0, 67, 0, 65, 0, 69, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 65, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 69, 0, 67, 0, 69, 0, 74, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 69, 0, 67, 0, 65, 0, 69, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 65, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 69, 0, 67, 0, 69, 0, 74, 0, 0, 0, 62, 0, 0, 65, 0, 0, 67, 0, 0, 0, 0, 0, 0, 0, 69, 0, 67, 0, 65, 0, 69, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 0, 67, 0, 65, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 60, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 60, 0, 190, 1, 0, 0, 190, 1, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 190, 1, 0, 0, 193, 1, 60, 0, 194, 1, 0, 0, 193, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 60, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 193, 1, 60, 0, 194, 1, 0, 0, 193, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 60, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 190, 1, 0, 0, 190, 1, 62, 0, 190, 1, 0, 0, 193, 1, 60, 0, 194, 1, 0, 0, 193, 1, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 190, 190, 190, 190, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 0, 0, 74, 0, 190, 190, 190, 190, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 0, 74, 0, 0, 0, 74, 0, 190, 190, 190, 190, 0, 0, 0, 0},
};

SynthParams synthParams[] = {
    {.attack = 109, .decay = 128, .sustain = 128, .release = 64, .waveform = 0, .transpose = 74, .volume = 119, .filtType = 3, .filtRes = 128, .filtFreq = 29, .pitchDrop = 83},
    {.attack = 30, .decay = 33, .sustain = 128, .release = 0, .waveform = 3, .transpose = 60, .volume = 35, .filtType = 0, .filtRes = 111, .filtFreq = 34, .pitchDrop = 0},
    {.attack = 102, .decay = 64, .sustain = 64, .release = 52, .waveform = 2, .transpose = 60, .volume = 77, .filtType = 1, .filtRes = 24, .filtFreq = 54, .pitchDrop = 0},
    {.attack = 0, .decay = 128, .sustain = 1, .release = 64, .waveform = 2, .transpose = 60, .volume = 30, .filtType = 2, .filtRes = 64, .filtFreq = 83, .pitchDrop = 0},
    {.attack = 89, .decay = 92, .sustain = 24, .release = 64, .waveform = 1, .transpose = 72, .volume = 33, .filtType = 2, .filtRes = 64, .filtFreq = 83, .pitchDrop = 0},
    {.attack = 64, .decay = 128, .sustain = 128, .release = 128, .waveform = 2, .transpose = 36, .volume = 128, .filtType = 0, .filtRes = 64, .filtFreq = 33, .pitchDrop = 0},
    {.attack = 111, .decay = 74, .sustain = 0, .release = 64, .waveform = 3, .transpose = 0, .volume = 2, .filtType = 1, .filtRes = 128, .filtFreq = 128, .pitchDrop = 0},
    {.attack = 128, .decay = 27, .sustain = 0, .release = 79, .waveform = 3, .transpose = 0, .volume = 32, .filtType = 2, .filtRes = 64, .filtFreq = 102, .pitchDrop = 0},
};

int rng = 1;

enum EnvState
{
    Attacking = 0,
    Decaying = 1,
    Sustaining = 2,
    Releasing = 3,
};

typedef struct
{
    uint32_t oscPhase;
    int64_t envLevel;
    int64_t envState;
    int64_t freq;
    int64_t low, band;
    int64_t paramIndex;
} SynthState;

// UNINITIALIZED DATA (should be filled with zeros)

int64_t SinTable[8192];
int64_t PowTable[8192];
SynthState synthStates[8];
int64_t currentRow;

int64_t nonLinearMap(int x)
{
    return PowTable[255 - x - 64] >> 15;
}

void music_init()
{
    const int64_t K = 3294199; // round(math.pi * 2 / 8192 * (1 << 32)) but optimized with matlab
    int64_t X = (int64_t)(1) << 32;
    int64_t Y = 0;
    int64_t F = 67878804062;     // 2**((64+127-69)/12)*440/32000*(1 << 32)
    const int64_t C = 126684666; // round(2**(-1/12)*(1 << 27))
    for (int i = 0; i < 8192; i++)
    {
        SinTable[i] = Y >> 18;
        X -= (Y * K) >> 32;
        Y += (X * K) >> 32;
        PowTable[i] = F;
        F = (F * C) >> 27;
    }
}

void music_render(int16_t *buffer, int32_t samples)
{
    int64_t localRng = rng;
    memset32(buffer, 0, sizeof(int16_t) * samples * 2);
    for (int track = 0; track < MUSIC_CHANNELS; track++)
    {
        SynthState *s = &synthStates[track];
        int64_t envLevel = s->envLevel;
        int32_t envState = s->envState;
        int64_t freq = s->freq;
        int64_t oscPhase = s->oscPhase;
        int64_t low = s->low;
        int64_t band = s->band;
        uint8_t note = noteData[track][currentRow];
        if (note == 0)
        { // release note
            envState = Releasing;
        }
        else if (note > 1)
        {
            s->paramIndex = note & 0x80;
            note &= 0x7F;
            envLevel = 0;
            envState = Attacking;
        }
        // load params from state
        int64_t paramIndex = s->paramIndex;
        SynthParams *params = &synthParams[track * 2 + (paramIndex >> 7)];
        const int64_t dropFreq = 7381975;
        if (note > 1)
        {
            freq = PowTable[255 - (params->transpose + note)];
        }
        int64_t attack = nonLinearMap(params->attack);
        int64_t decay = nonLinearMap(params->decay);
        int64_t sustain = ((int64_t)params->sustain) << 14;
        int64_t release = nonLinearMap(params->release);
        int64_t pitchDrop = (int64_t)params->pitchDrop;
        int64_t waveform = params->waveform;
        int64_t volume = params->volume;
        int64_t transpose = params->transpose;
        int64_t filtType = params->filtType;
        int64_t filtRes = params->filtRes;
        int64_t filtFreq2 = params->filtFreq;
        filtFreq2 *= filtFreq2;
        for (int i = 0; i < samples; i++)
        {
            if (envState == Attacking)
            {
                envLevel += attack;
                if (envLevel >= 1 << 21)
                {
                    envState++;
                }
            }
            if (envState == Decaying)
            {
                envLevel -= decay;
                if (envLevel <= sustain)
                {
                    envState++;
                }
            }
            if (envState == Releasing)
            {
                envLevel -= release;
                if (envLevel < 0)
                {
                    envLevel = 0;
                }
            }
            // for (int i = 0; i < 1; i++)
            //            {
            int64_t res;
            oscPhase += freq;
            switch (waveform)
            {
            default: // sine
                res = SinTable[(oscPhase >> 19) & 8191];
                break;
            case 1: // square
                res = (oscPhase & 0x80000000) >> 16;
                break;
            case 2:                                              // saw
                res = (int64_t)(((int32_t)oscPhase) >> 16) >> 1; // done so that sign bits are shifted in correctly
                break;
            case 3:
                localRng *= 18007;
                res = localRng >> 49;
            }
            freq -= (freq - dropFreq) * pitchDrop >> 16;

            int64_t x = (res * volume * envLevel) >> 28;

            low += (filtFreq2 * band) >> 14;
            int64_t high = (filtRes * (x - band) >> 7) - low;
            band += (filtFreq2 * high) >> 14;
            int64_t out;
            switch (filtType)
            {
            default:
            case 0: // low
                out = low;
                break;
            case 1: // high
                out = high;
                break;
            case 2: // band
                out = band;
                break;
            case 3: // notch
                out = high + low;
                break;
            }
            buffer[i * 2 + 1] += out;
            buffer[i * 2] += out;
        }
        s->envLevel = envLevel;
        s->envState = envState;
        s->freq = freq;
        s->oscPhase = oscPhase;
        s->low = low;
        s->band = band;
    }
    rng = localRng;
    currentRow++;
}
