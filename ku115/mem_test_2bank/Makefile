#Repo containing utility and libs repository to use
COMMON_REPO := ../../../

# Make file Includes
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk

# Host files and setting
host_SRCS=./src/host.cpp $(xcl2_SRCS)
host_HDRS=$(xcl2_HDRS)
host_CXXFLAGS= -I./src/ -std=c++0x -I./src/ $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS) 
host_LDFLAGS=$(opencl_LDFLAGS)

EXES=host

# Kernel
mem_test_SRCS=./src/mem_test.cpp
mem_test_CLFLAGS=-k mem_test -I./src --max_memory_ports mem_test

XOS=mem_test

# xclbin
mem_test_XOS=mem_test
mem_test_LDCLFLAGS=--xp misc:map_connect=add.kernel.mem_test_1.M_AXI_GMEM0.core.OCL_REGION_0.M00_AXI --xp misc:map_connect=add.kernel.mem_test_1.M_AXI_GMEM1.core.OCL_REGION_0.M01_AXI
#mem_test_NDEVICES=xilinx:adm-pcie-7v3:1ddr
#mem_test_N
DEVICES=xilinx:xil-accel-rd-ku115:4ddr-xpr

XCLBINS=mem_test

# check
check_EXE=host
check_XCLBINS=mem_test
check_NDEVICES=$(mem_test_NDEVICES)

CHECKS=check
TARGETS=hw

include $(COMMON_REPO)/utility/rules.mk

