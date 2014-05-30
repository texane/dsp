#!/usr/bin/env sh

./a.out \
-fsampl 100000 \
-tsampl 1:2 \
-filter 0:0.01:0 \
-filter 0.01:50000:2.0 \
-ifile 0000.dat \
-ofile /tmp/o.dat \
-icol 2
