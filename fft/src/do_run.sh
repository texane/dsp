#!/usr/bin/env sh
./a.out > /tmp/mangled_name.o ;
gnuplot -e "plot '/tmp/mangled_name.o' with impulses; replot; pause mouse key;" ;
