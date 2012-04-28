#!/usr/bin/env sh
# ./a.out <duration_ms> <w0,a0,phi0> ...
# ./a.out 1000 2000,400,0 4000,400,0
./a.out $@ | sox -r 44100 -e signed -b 16 -c 2 -t raw - tonegen.wav
