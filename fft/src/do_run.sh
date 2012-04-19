#!/usr/bin/env sh

./a.out > /tmp/mangled_name.o ;

# plot frequency domain
# gnuplot -e "plot '/tmp/mangled_name.o' with impulses; replot; pause mouse key;" ;

# plot time domain
gnuplot -e "plot '/tmp/mangled_name.o' with lines; replot; pause mouse key;" ;
