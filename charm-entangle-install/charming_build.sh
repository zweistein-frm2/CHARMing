#!/bin/bash
rm -R out
mkdir out
cmake -S CHARMing/ -B out/
cd out
make
#make package



