#!/bin/sh
CC=cc
PRG=mtkview
set -e
set -x
$CC $(pkg-config --cflags x11 xrender libturbojpeg) -funsigned-char -o $PRG -Wall -O3 $PRG.c $(pkg-config --libs x11 xrender libturbojpeg) -lm -DDMTK_TURBOJPEG
#without libjpeg-turbo support
#$CC $(pkg-config --cflags x11 xrender) -funsigned-char -o $PRG -Wall -O3 $PRG.c $(pkg-config --libs x11 xrender) -lm
