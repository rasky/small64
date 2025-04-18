#if VIDEO_TYPE == 1 || VIDEO_TYPE == 2
// NTSC and MPAL are 60hz
#define TTT(fc)         (fc)
#else
// PAL is 50 Hz
#define TTT(fc)         ((fc) * 50 / 60)
#endif

// Parts of the intro: frames at which the various parts begin
#define T_START         TTT(0)
#define T_INTRO         TTT(400/2)
#define T_FRACTAL       TTT(850/2)
#define T_MESH          TTT(1700/2+50)
#define T_CREDITS       TTT(2200/2)
#define T_ANIMATE       TTT(3100/2)
#define T_MESH2         TTT(3500/2)
#define T_ANIMSTOP      TTT(4030/2)  // ~930 after T_ANIMATE
