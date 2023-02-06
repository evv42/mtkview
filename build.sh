#!/bin/sh
CC=gcc
PRG=mtkview
set -e
set -x
$CC -Wall -funsigned-char -o  $PRG -Wall -O3 $PRG.c -lm -lX11 -lXrender
