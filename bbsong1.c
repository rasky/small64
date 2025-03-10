#define SONG_FREQUENCY      32000


static float pow2_approx(float n) {
    const float ln2 = 0.693147f;
    float x = n * ln2;
    float result = 1.0f + x + (x * x) / 2.0f + (x * x * x) / 6.0f;
    return result;
}

__attribute__((noinline))
float bbgen(int t)
{
    __attribute__((used))
    static float pow2s[] = {
        1.1071389336054636, 1.1729728404832946, 1.2427214351778433, 1.3166174971401423, 1.3949076476076858, 1.4778531726798843, 1.5657308953361322, 1.6588340993067685, 1.7574735078802632, 1.861978320913309, 1.9726973135047394, 2.09, 2.214277867210927, 2.3459456809665893, 2.4854428703556866, 2.6332349942802846, 2.7898152952153716, 2.9557063453597685, 3.1314617906722644, 3.3176681986135366, 3.5149470157605265, 3.723956641826618, 3.9453946270094784, 4.18
    };

    float s(int t)
    {
        static const int8_t rel[] = {-7,-11,-2,-4,-9,10,12};
        static const char x[] = "80a50a80a50a805af0a50bf1a73cf3c7g0a80ag0a80ag08ag0a80ag0f137c3578db5db8da5dbfd5a7ce3ce7ce3ec875fg0a80ag0a80ag08ag0a80ag0f137c357";

        if (t < 0) t = 0;        
        char ch = x[(t>>12)&127];
        int val = (ch >= 'a') ? rel[ch-'a'] : ch-'0';
        return ((int)(2.09f * t * pow2_approx(val/12.0f)) & 128) * (1-t%4096/4096.0f);
        // return ((int)(pow2s[val+11]*t) & 128) * (1-t%4096/4096.0f);
    }
    
    return s(t) + s(t-8192)/2+ s(t-16384)/4;
}
