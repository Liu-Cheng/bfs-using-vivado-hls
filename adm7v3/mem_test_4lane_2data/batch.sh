#!/bin/sh

# Aligned memory access
sed -i "/int stride/c\int stride = 1024;" ./src/host.cpp
compile
run > tmp.txt
grep 'bandwidth' ./tmp.txt >> aligned_bandwidth.txt
grep 'latency' ./tmp.txt >> aligned_latency.txt

# Unaligned memory access
sed -i "/int stride/c\int stride = 1379;" ./src/host.cpp
compile
run > tmp.txt
grep 'bandwidth' ./tmp.txt >> unaligned_bandwidth.txt
grep 'latency' ./tmp.txt >> unaligned_latency.txt

