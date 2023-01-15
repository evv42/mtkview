#!/bin/sh
CC=cc
PRG=mtkview
set -e
set -x
$CC -funsigned-char -o $PRG -Wall $PRG.c -lm -lX11
