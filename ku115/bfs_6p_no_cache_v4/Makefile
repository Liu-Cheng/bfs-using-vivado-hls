#Repo containing utility and libs repository to use
COMMON_REPO := ../../../

# Make file Includes
include $(COMMON_REPO)/utility/boards.mk
include $(COMMON_REPO)/libs/xcl2/xcl2.mk
include $(COMMON_REPO)/libs/opencl/opencl.mk

# Host files and setting
host_SRCS=./src/host.cpp ./src/graph.cpp $(xcl2_SRCS)
host_HDRS=$(xcl2_HDRS)
host_CXXFLAGS=-I./src/ -std=c++0x -I./src/ $(xcl2_CXXFLAGS) $(opencl_CXXFLAGS) 
host_LDFLAGS=$(opencl_LDFLAGS)

EXES=host

# Kernel
bfs_SRCS=./src/bfs.cpp
bfs_CLFLAGS=-k bfs -I./src --max_memory_ports bfs

XOS=bfs

# xclbin
bfs_XOS=bfs
bfs_LDCLFLAGS=--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM0.core.OCL_REGION_0.M00_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM1.core.OCL_REGION_0.M01_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM2.core.OCL_REGION_0.M02_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM3.core.OCL_REGION_0.M03_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM4.core.OCL_REGION_0.M00_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM5.core.OCL_REGION_0.M01_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM6.core.OCL_REGION_0.M02_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM7.core.OCL_REGION_0.M03_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM8.core.OCL_REGION_0.M00_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM9.core.OCL_REGION_0.M01_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM10.core.OCL_REGION_0.M02_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM11.core.OCL_REGION_0.M03_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM12.core.OCL_REGION_0.M00_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM13.core.OCL_REGION_0.M01_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM14.core.OCL_REGION_0.M02_AXI \

#bfs_NDEVICES=xilinx:adm-pcie-7v3:1ddr
#bfs_N
DEVICES=xilinx:xil-accel-rd-ku115:4ddr-xpr

XCLBINS=bfs

# check
check_EXE=host
check_XCLBINS=bfs
check_NDEVICES=$(bfs_NDEVICES)

CHECKS=check
TARGETS=hw

include $(COMMON_REPO)/utility/rules.mk

