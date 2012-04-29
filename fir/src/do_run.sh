#!/usr/bin/env sh

./a.out > /tmp/mangled_name.o ;

# plot frequency domain
# gnuplot -e "plot '/tmp/mangled_name.o' with impulses; replot; pause mouse key;" ;

# plot time domain
# gnuplot -e "plot '/tmp/mangled_name.o' using 1:3 with lines,'' using 1:2 with lines; replot; pause mouse key;" ;
# gnuplot -e "plot '/tmp/mangled_name.o' using 1:2 with lines; replot; pause mouse key;" ;
