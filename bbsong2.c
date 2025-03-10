// 1kdance by raphaelgoulart

#define SONG_FREQUENCY      11025

static float bbgen(int t)
{
    const float z = 40.7f;
    float b = t / 2250.f;
    int r = (int)(b);
    int y = r % 16;
    static const float arr1[16] = {1, 2, 1, 2, 1.2f, 2.4f, 1.2f, 2.4f, 1.33f, 2.67f, 1.33f, 2.67f, 1.5f, 3, 1.5f, 3};
    float a = arr1[y];
    int c = r % 64 > 1 && r % 64 < 33;
    float o = c ? 1.19 : 1.5;
    static float n[32] = {
        0, 0, 2.38f, 2.67f, 2.38f, 0, 2, 2.24f, 0, 2.38f, 0, 2.24f, 0, 1.78f, 0, 2, 2,
        2, 1, 1.19f, 1.5f, 0, 1.19f, 1.33f, 0, 1.19f, 0, 0, 0, 0, 0, 0
    };
    n[29]=n[30]=n[31]=o;
    n[27]=c ? 1.12f : 1.33f;

    int x = (int)(r / 4) % 4;
    static const float arr2[] = {12, 12, 10.67f, 12};
    float d = arr2[x];
    static const float arr3[] = {9.52, 9.52, 8, 8.96};
    float g = arr3[x];
    static const float arr4[] = {8, 7.12, 6.72, 7.52};
    float h = arr4[x];
    float v = remf(b * 2, 4) * 1.25f;
    int w = (int)(y / 12);
    int u = (int)(r / 16) % 4;
    static const float j[4][2] = {
        {19.04, 17.92},
        {14.24, 9.52},
        {10.64, 12},
        {8, 0},
    };

    return 40 +
        mysinf(t * (1 / z) + mysinf(t * 1.125f / z) * 7 * (1 - remf(b * 2, 4) > 0 ? 1 - remf(b * 2, 4) : 0)) * (40 - remf(b * 15, 30)) +
        (b < 16 ? 0 : random() * (r % 4 == 2 ? 32 - remf(b * 32, 32) : 0)) +
        (b < 32 ? 0 : random() * (16 - (remf(b * 28, 28) < 16 ? remf(b * 28, 28) : 16))) +
        (b < 64 ? 0 : mysinf(t * a / z + mysinf(t * a / z) * 4 * (1 - remf(b, 1))) * 32 + 32) +
        (b < 96 ? 0 : mysinf(t * d / z + mysinf(t * d / z) * (0 + remf(b * 1.5f, 3))) * v + 5) +
        (b < 96 ? 0 : mysinf(t * g / z + mysinf(t * g / z) * (0 + remf(b * 1.5f, 3))) * v + 5) +
        (b < 96 ? 0 : mysinf(t * h / z + mysinf(t * h / z) * (0 + remf(b * 1.5f, 3))) * v + 5) +
        (b < 128 ? 0 : remf(t * n[r % 32] * 8, 256) > 121 + abs(108 - remf(b * 56, 224)) ? 20 : 0) +
        (b < 129 ? 0 : remf(t * n[(r - 1) % 32] * 7.98f, 256) > 121 + abs(108 - remf(b * 56, 224)) ? 7 : 0) +
        (b < 192 ? 0 : mysinf(
            t * j[u][w] / z + mysinf(t * j[u][w] * 2 / z) * 4 * (y < 2 || (y >= 12 && y < 14) ? 1 - remf(b / 2, 1) : 0)
        ) * (5.3f - y / 3 % 4) + 5.3f);
}

