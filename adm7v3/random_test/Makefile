COMMON_REPO := ../../../

# wide Memory Access Application
include ${COMMON_REPO}/utility/boards.mk
include ${COMMON_REPO}/libs/xcl/xcl.mk
include ${COMMON_REPO}/libs/opencl/opencl.mk
# cnn Host Application
host_SRCS=./src/host.cpp $(xcl_SRCS)
host_HDRS=$(xcl_HDRS)
host_CXXFLAGS=-I./src/ $(xcl_CXXFLAGS) $(opencl_CXXFLAGS) -std=c++0x
host_LDFLAGS=$(opencl_LDFLAGS)
host_CXXFLAGS+=-Wall

EXES=host

# cnn Kernels
transfer_SRCS=./src/transfer.cl

XOS=transfer

# cnn xclbins
transfer_XOS=transfer

XCLBINS=transfer

# check
check_EXE=host
check_XCLBINS=transfer

CHECKS=check

DEVICES=xilinx:adm-pcie-7v3:1ddr:3.0
TARGETS=hw

include $(COMMON_REPO)/utility/rules.mk
