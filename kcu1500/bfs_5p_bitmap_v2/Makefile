#Repo containing utility and libs repository to use
COMMON_REPO := ../../..

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
bfs_CLFLAGS=-k bfs -I./src 
#bfs_CLFLAGS=-k bfs -I./src --max_memory_ports bfs

XOS=bfs

# xclbin
bfs_XOS=bfs
bfs_LDCLFLAGS=--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM0.core.OCL_REGION_0.M00_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM1.core.OCL_REGION_0.M01_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM20.core.OCL_REGION_0.M00_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM21.core.OCL_REGION_0.M01_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM22.core.OCL_REGION_0.M02_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM23.core.OCL_REGION_0.M03_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM24.core.OCL_REGION_0.M00_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM25.core.OCL_REGION_0.M01_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM26.core.OCL_REGION_0.M02_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM27.core.OCL_REGION_0.M03_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM28.core.OCL_REGION_0.M00_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM29.core.OCL_REGION_0.M01_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM2a.core.OCL_REGION_0.M02_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM2b.core.OCL_REGION_0.M03_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM2c.core.OCL_REGION_0.M00_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM2d.core.OCL_REGION_0.M01_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM2e.core.OCL_REGION_0.M02_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM2f.core.OCL_REGION_0.M03_AXI \
	--xp misc:map_connect=add.kernel.bfs_1.M_AXI_GMEM3.core.OCL_REGION_0.M02_AXI \

#bfs_NDEVICES=xilinx:adm-pcie-7v3:1ddr
#DEVICES=xilinx:xil-accel-rd-ku115:4ddr-xpr
DEVICES=xilinx:kcu1500:4ddr-xpr:4.0

XCLBINS=bfs

# check
check_EXE=host
check_XCLBINS=bfs
check_NDEVICES=$(bfs_NDEVICES)

CHECKS=check
TARGETS=sw_emu

include $(COMMON_REPO)/utility/rules.mk

