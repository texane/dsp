#!/usr/bin/gnuplot -persist
set grid
set yrange[-3:3]
plot \
'dat' using 1:2 title 'x' with linespoints lc 18 pt 18 ps 0.3, \
'dat' using 1:3 title 'y0' with linespoints lc 1 pt 3 ps 1, \
'dat' using 1:4 title 'y1' with linespoints lc 3 pt 18 ps 0.3