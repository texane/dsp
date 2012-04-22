#!/usr/bin/env sh

# salsa
# ALIB_LFLAGS="-L$HOME/install/lib -lsalsa"
# alsa
ALIB_LFLAGS="-lasound"

gcc -Wall -O3 -I. main.c x.c ui.c $ALIB_LFLAGS -lm -lfftw3 -lSDL