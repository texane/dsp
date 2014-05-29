#!/usr/bin/env sh

gcc -O2 -Wall fft.c ../common/csv.c -lm -lfftw3
