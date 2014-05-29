#!/usr/bin/env gnuplot

set xrange [-100:]

set boxwidth 0.5
set style fill solid

set term png
set output '/tmp/o.png'

plot "/tmp/o.dat" using 1:2 with boxes
