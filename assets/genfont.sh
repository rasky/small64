#!/bin/bash
python3 microfont.py Tomorrow-Night.png Tomorrow-Night-chars.txt 1 1 11 16 \
    --phrase phrases.txt



#python3 microfont.py oaknut2.png oaknut2-chars.txt 0 0 11 17 scroller.txt
#echo "#define CHAR_WIDTH 11" >font.c.inc
#echo "#define CHAR_HEIGHT 13" >>font.c.inc

# echo "#define CHAR_WIDTH 8" >font.c.inc
# echo "#define CHAR_HEIGHT 11" >>font.c.inc

# python3 microfont.py twenty_goto_ten.png twenty_goto_ten_chars.txt 1 1 7 9 scroller.txt
# echo "#define CHAR_WIDTH 6" >font.c.inc
# echo "#define CHAR_HEIGHT 8" >>font.c.inc

# convert output_atlas.png GRAY:output_atlas.bin
# xxd -i -n font output_atlas.bin >>font.c.inc
# xxd -i widths.bin >>font.c.inc
# xxd -i phrase.bin >>font.c.inc
# sed -i -e 's/unsigned /const unsigned /g' font.c.inc
