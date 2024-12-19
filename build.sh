#!/bin/sh
CC=cc
PRG=mtkview
#OPT="-msse2 -g"
set -e
set -x
$CC $(pkg-config --cflags x11 libturbojpeg) -funsigned-char -o $PRG -Wall -O3 $OPT $PRG.c $(pkg-config --libs x11 libturbojpeg) -lm -lXrender -DDMTK_TURBOJPEG
#without libjpeg-turbo support
#$CC $(pkg-config --cflags x11) -funsigned-char -o $PRG -Wall -O3 $OPT $PRG.c $(pkg-config --libs x11) -lm -lXrender
#also, can add OPT if desired
