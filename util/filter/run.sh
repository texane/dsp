#!/usr/bin/env sh

./a.out \
-fsampl 100000 \
-tsampl_lo 1 \
-tsampl_hi 5 \
-fband_lo 0 \
-fband_hi 1 \
-ifile 0000.dat \
-ofile /tmp/o.dat \
-icol 1
