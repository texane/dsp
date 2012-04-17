#!/usr/bin/gnuplot -persist
set grid
set yrange[-3:3]
plot \
'dat' using 1:2 title 'x' with points lc 18 pt 18 ps 1, \
'dat' using 1:3 title 'y0' with points lc 1 pt 18 ps 1, \
'dat' using 1:4 title 'y1' with points lc 3 pt 18 ps 1
