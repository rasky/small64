#include "minimath.h"
#include <stdint.h>
#define SONG_FREQUENCY 32000
#define MUSIC_BPM 144
#define MUSIC_LENGTH 1092

typedef struct
{
    uint8_t sustain, release;
    uint8_t waveform, freqMult, volume; // 0-3
    uint8_t filtType, filtFreq;
    uint8_t pitchDrop; // 0-128, 0 no drop
} SynthParams;

static const uint8_t noteData[] = {192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 64, 0, 64, 0, 64, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 64, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 64, 0, 64, 0, 64, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 64, 0, 64, 0, 64, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 64, 0, 64, 0, 64, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 61, 0, 57, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 61, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 61, 0, 57, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 61, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 61, 0, 57, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 61, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 61, 0, 57, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 61, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 61, 0, 57, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 61, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 54, 0, 0, 55, 0, 0, 57, 0, 55, 0, 0, 0, 59, 0, 0, 0, 57, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 66, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 61, 0, 57, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 61, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 54, 0, 0, 55, 0, 0, 57, 0, 55, 0, 0, 0, 59, 0, 0, 0, 57, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 66, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 61, 0, 57, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 0, 59, 0, 61, 0, 0, 0, 64, 0, 0, 61, 0, 0, 59, 0, 0, 0, 0, 0, 0, 0, 57, 0, 59, 0, 57, 0, 52, 0, 0, 0, 54, 0, 0, 55, 0, 0, 57, 0, 55, 0, 0, 0, 59, 0, 0, 0, 57, 0, 0, 0, 0, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 66, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 66, 0, 192, 0, 0, 0, 192, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 192, 0, 0, 0, 189, 0, 66, 0, 188, 0, 0, 0, 189, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 66, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 189, 0, 66, 0, 188, 0, 0, 0, 189, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 66, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 189, 0, 66, 0, 188, 0, 0, 0, 189, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 66, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 189, 0, 66, 0, 188, 0, 0, 0, 189, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 66, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 189, 0, 66, 0, 188, 0, 0, 0, 189, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 66, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 64, 0, 192, 0, 0, 0, 192, 0, 0, 0, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 192, 192, 192, 192, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 192, 192, 192, 192, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 0, 0, 64, 0, 192, 192, 192, 192, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 192, 192, 192, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 0, 0, 64, 64, 64, 64, 64, 64, 192, 192, 192, 192, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 0, 64, 64, 64, 64, 64, 64, 192, 192, 192, 192, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

typedef struct
{
    SynthParams params[2];
    int64_t freq;
    uint32_t oscPhase;
    int32_t envLevel;
    int32_t envSustain;
    int32_t low, band;
    int32_t paramIndex;
    int32_t clear;
    int32_t end;
} SynthState;

SynthState Synths[] = {
    {
        .params = {
            {.sustain = 66 + 3, .release = 32 + 3, .waveform = 0, .freqMult = 8, .volume = 99 / 2, .filtType = 0, .filtFreq = 2, .pitchDrop = 83},
            {.sustain = 122, .release = 122, .waveform = 2, .freqMult = 4, .volume = 9 * 2, .filtType = 1, .filtFreq = 38, .pitchDrop = 0},
        },
        .clear = 1,
    },
    {
        .params = {
            {.sustain = 0 + 3, .release = 70 + 3, .waveform = 1, .freqMult = 4, .volume = 18, .filtType = 0, .filtFreq = 11, .pitchDrop = 0},
            {.sustain = 104 + 3, .release = 0 + 3, .waveform = 1, .freqMult = 4, .volume = 22 * 2, .filtType = 1, .filtFreq = 86, .pitchDrop = 0},
        },
    },
    {.params = {
         {.sustain = 0 + 3, .release = 64 + 3, .waveform = 0, .freqMult = 8, .volume = 96 * 2 / 2, .filtType = 1, .filtFreq = 32, .pitchDrop = 0},
         {.sustain = 66 + 3, .release = 32 + 3, .waveform = 1, .freqMult = 1, .volume = 96 * 2, .filtType = 1, .filtFreq = 3, .pitchDrop = 0},
     }},
    {
        .params = {
            {.sustain = 0 + 3, .release = 53 + 3, .waveform = 2, .freqMult = 0, .volume = 3, .filtType = 0, .filtFreq = 128, .pitchDrop = 0},
            {.sustain = 0 + 3, .release = 64 + 3, .waveform = 2, .freqMult = 0, .volume = 22 * 2, .filtType = 1, .filtFreq = 81, .pitchDrop = 0},
        },
    },
    {
        // Terminator Synth
        .end = 1,
    }};

int32_t rng = 1;

// UNINITIALIZED DATA (should be filled with zeros)

int64_t SinTable[8192];
int64_t PowTable[8192];
int32_t currentRow;

static void music_init(void)
{
    const int64_t K = 3294199; // round(math.pi * 2 / 8192 * (1 << 32)) but optimized with matlab
    int64_t X = (int64_t)(1) << 32;
    int64_t Y = 0;
    int64_t F = 0x12d00000;       // 67878804062;     // 2**((64+127-69)/12)*440/32000*(1 << 32)
    const int64_t C = 4053909305; // round(2**(-1/12)*(1 << 32))
    for (int i = 0; i < 8192; i++)
    {
        SinTable[i] = Y;
        X -= (Y * K) >> 32;
        Y += (X * K) >> 32;
        PowTable[i] = F;
        F = (F * C) >> 32;
    }
}

static void music_render(int16_t *buffer)
{
    int32_t pos = currentRow;

    SynthState *s = &Synths[0];

    while (s->end == 0)
    {
        int64_t oscPhase = s->oscPhase;
        int32_t envLevel = s->envLevel;
        int32_t envSustain = s->envSustain;
        int64_t freq = s->freq;
        int32_t low = s->low;
        int32_t band = s->band;
        uint32_t note = noteData[pos];
        // load params from state
        if (note)
        {
            s->paramIndex = note >> 7;
            note &= 0x7F;
            envSustain = 1 << 21;
            envLevel = 1 << 21;
            oscPhase = 0;
            freq = PowTable[note];
        }
        SynthParams *params = &s->params[s->paramIndex];
        const int32_t dropFreq = 0x700000 / 8; // rounded into nice hex number: 7381975;

        int16_t *ptr = buffer;
        int16_t *end = buffer + AI_BUFFER_SIZE / 2;
        while (ptr < end)
        {
            envSustain -= PowTable[params->sustain + 168];
            if (envSustain < 0)
            {
                envLevel -= PowTable[params->release + 168];
                if (envLevel < 0)
                {
                    envLevel = 0;
                }
            }
            int64_t res;
            freq -= (freq - dropFreq) * params->pitchDrop >> 16;
            oscPhase += freq * params->freqMult;
            switch (params->waveform)
            {
            case 0:
                res = SinTable[(uint32_t)oscPhase >> 19];
                break;
            case 1:                                   // saw
                res = (int64_t)(((int32_t)oscPhase)); // done so that sign bits are shifted in correctly
                break;
            default:
            case 2:
                rng *= 18007;
                res = rng;
            }

            // NOTE: x/low/high/band were originally all 64-bit, but were
            // brutally truncated to 32-bit during optimisation to save 72 bytes.

            int32_t x = (res * params->volume * envLevel) >> 45;

            int32_t filtFreq = params->filtFreq;

            low += (filtFreq * band) >> 6;
            int32_t high = x - band - low;
            band += (filtFreq * high) >> 8;

            int32_t out = ptr[0];
            // Skip loading previous value during the first track, to clear the buffer
            if (s->clear)
                out = 0;

            switch (params->filtType)
            {
            case 0: // high
                out += high;
                break;
            default:
            case 1: // band
                out += band;
                break;
            }
            ptr[0] = out;
            ptr[1] = out;
            ptr += 2;
        }
        s->oscPhase = oscPhase;
        s->envLevel = envLevel;
        s->envSustain = envSustain;
        s->freq = freq;
        s->low = low;
        s->band = band;
        pos += MUSIC_LENGTH;
        s++;
    }

    currentRow++;
}
