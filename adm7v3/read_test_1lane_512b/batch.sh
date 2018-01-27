#!/bin/sh

# Aligned memory access
sed -i "/int stride/c\int stride = 1024;" ./src/host.cpp
for i in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576
do
	sed -i "/int burst_len/c\int burst_len = $i;" ./src/host.cpp
	compile
	run > tmp.txt
	grep 'bandwidth' ./tmp.txt >> aligned_bandwidth.txt
	grep 'latency' ./tmp.txt >> aligned_latency.txt
done

# Unaligned memory access
sed -i "/int stride/c\int stride = 1379;" ./src/host.cpp
for i in 1 2 4 8 16 32 64 128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576
do
	sed -i "/int burst_len/c\int burst_len = $i;" ./src/host.cpp
	compile
	run > tmp.txt
	grep 'bandwidth' ./tmp.txt >> unaligned_bandwidth.txt
	grep 'latency' ./tmp.txt >> unaligned_latency.txt
done

