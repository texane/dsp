#!/usr/bin/env sh

gcc -DCONFIG_PERROR -O2 -Wall filter.c ../common/csv.c -lm -lfftw3
