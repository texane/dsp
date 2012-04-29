#!/usr/bin/env sh
gcc -Wall -O3 -I../tonegen main.c ../tonegen/tonegen.c -lm -lfftw3
