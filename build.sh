#!/bin/sh
CC=gcc
PRG=mtkview
set -e
set -x
$CC -Wall -funsigned-char -o $PRG -Wall -O3 -msse2 -g $PRG.c -lm -lX11 -lXrender -lturbojpeg -DDMTK_TURBOJPEG
#without libjpeg-turbo support
#$CC -Wall -funsigned-char -o $PRG -Wall -O3 -g $PRG.c -lm -lX11 -lXrender
#also, can add -msse2 if desired
