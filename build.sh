#!/bin/sh
CC=cc
PRG=mtkview
set -e
set -x
$CC -o $PRG -Wall -O2 $PRG.c -lm -lX11
