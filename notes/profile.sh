#!/bin/bash

set -e

./build/fred ~/repos/fren/src/main.c

# ./build/fred ~/repos/fred/files/file2.txt

echo "---------------------------------------------------------------------------------" >> profile_results.txt
date >> profile_results.txt
gprof --flat-profile ./build/fred >> profile_results.txt

