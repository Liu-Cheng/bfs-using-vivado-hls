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
bfs_SRCS=
bfs_CLFLAGS=

XOS=

# xclbin
bfs_XOS=

XCLBINS=

# check
check_EXE=host
check_XCLBINS=
DEVICES=xilinx:adm-pcie-7v3:1ddr:3.0
TARGETS=sw_emu

CHECKS=check

include $(COMMON_REPO)/utility/rules.mk

