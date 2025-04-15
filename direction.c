#if VIDEO_NTSC
#define TTT(fc)         (fc)
#else
#define TTT(fc)         ((fc) * 50 / 60)   
#endif

// Parts of the intro: frames at which the various parts begin
#define T_START         TTT(0)
#define T_INTRO         TTT(400)
#define T_FRACTAL       TTT(850)
#define T_MESH          TTT(1700)
#define T_CREDITS       TTT(2200)
#define T_ANIMATE       TTT(3100)