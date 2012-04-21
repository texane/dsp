#!/usr/bin/env sh

# salsa
# ALIB_LFLAGS="-L$HOME/install/lib -lsalsa"
# alsa
ALIB_LFLAGS="-lasound"

gcc -Wall -O3 main.c $ALIB_LFLAGS -lm -lfftw3