#!/usr/bin/env sh
$HOME/install/bin/gmeteor ../../fir/lowpass_6000.gmeteor > /tmp/fu.h ;
# gnuplot -e "plot '/tmp/fu.plot'; pause mouse key;" ;
> /tmp/bar.h < /tmp/fu.h sed ':a;N;$!ba;s/\n/, /g'
gcc -Wall main.c -lm -lfftw3 ;
