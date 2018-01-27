COMMON_REPO := ../../../

include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl/xcl.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk

# Host Application
host_SRCS=./src/host.cpp $(xcl_SRCS)
host_HDRS=$(xcl_HDRS)
host_CXXFLAGS=-I./src/ $(xcl_CXXFLAGS) $(opencl_CXXFLAGS) -std=c++0x 
host_LDFLAGS=$(opencl_LDFLAGS)

EXES=host

# Kernel
mem_test_SRCS=./src/mem_test.cpp
mem_test_CLFLAGS= -k mem_test

XOS=mem_test

# xclbin
mem_test_XOS=mem_test

XCLBINS=mem_test

# check
check_EXE=host
check_XCLBINS=mem_test
DEVICES=xilinx:xil-accel-rd-ku115:4ddr-xpr
#DEVICES=xilinx:adm-pcie-7v3:1ddr:3.0
TARGETS=hw

CHECKS=check

include $(COMMON_REPO)/utility/rules.mk

