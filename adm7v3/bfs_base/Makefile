COMMON_REPO := ../../../

include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl/xcl.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk

# Host Application
host_SRCS=./src/host.cpp ./src/graph.cpp  $(xcl_SRCS)
host_HDRS=$(xcl_HDRS)
host_CXXFLAGS=-I./src/ $(xcl_CXXFLAGS) $(opencl_CXXFLAGS) -std=c++0x 
host_LDFLAGS=$(opencl_LDFLAGS)

EXES=host

# Kernel
bfs_k0_SRCS=./src/bfs_k0.cpp
bfs_k0_CLFLAGS= -k bfs_k0

bfs_k1_SRCS=./src/bfs_k1.cpp
bfs_k1_CLFLAGS= -k bfs_k1

XOS=bfs_k0 bfs_k1 

# xclbin
bfs_XOS=bfs_k0 bfs_k1
bfs_LDCLFLAGS=-k bfs_k0 -k bfs_k1 

XCLBINS=bfs

# check
check_EXE=host
check_XCLBINS=bfs
DEVICES=xilinx:adm-pcie-7v3:1ddr:3.0
TARGETS=hw

CHECKS=check

include $(COMMON_REPO)/utility/rules.mk

