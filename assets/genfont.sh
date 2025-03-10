#!/bin/bash
python3 microfont.py oaknut2.png oaknut2-chars.txt scroller.txt
convert output_atlas.png GRAY:output_atlas.bin
xxd -i -n font output_atlas.bin >font.c.inc
xxd -i widths.bin >>font.c.inc
xxd -i phrase.bin >>font.c.inc
sed -i -e 's/unsigned /const unsigned /g' font.c.inc
