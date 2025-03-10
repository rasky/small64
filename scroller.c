#include <stdint.h>
#include "assets/font.c.inc"

#define CHAR_WIDTH  11
#define CHAR_HEIGHT 13
#define CHAR_SIZE   (CHAR_WIDTH * CHAR_HEIGHT)

static void draw_char(int index, int x, int y, uint16_t *FB) {
    for (int j = 0; j < CHAR_HEIGHT; j++) {
        for (int i = 0; i < CHAR_WIDTH; i++) {
            if (x+i > 0 && x+i<320 && font[index * CHAR_SIZE + j * CHAR_WIDTH + i]) {
                FB[(y + j*2+2) * 320 + x + i + 2] = 0x0001;
                FB[(y + j*2+3) * 320 + x + i + 2] = 0x0001;
                FB[(y + j*2+0) * 320 + x + i] = 0xffff;
                FB[(y + j*2+1) * 320 + x + i] = 0xffff;
            }
        }
    }
}

static void draw_scroller(uint16_t *FB)
{
    static int xpos0 = 500;

    int xpos = xpos0;
    int ypos = 150;
    const uint8_t *text = phrase_bin;

    for (int i=0; i<phrase_bin_len; i++) {
        if (xpos > -CHAR_WIDTH && text[i] != 0) {
            draw_char(text[i] - 1, xpos, ypos, FB);
        }
        xpos += widths_bin[text[i]]+1; 
        if (xpos > 320) break;
    }

    xpos0 -= 2;
}
