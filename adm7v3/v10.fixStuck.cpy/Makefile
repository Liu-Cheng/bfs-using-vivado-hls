COMMON_REPO := ../../

#Common Includes
include ${COMMON_REPO}/utility/boards.mk
include $(COMMON_REPO)/libs/cmdparser/cmdparser.mk
include $(COMMON_REPO)/libs/logger/logger.mk
include $(COMMON_REPO)/libs/oclHelper/oclHelper.mk
include ${COMMON_REPO}/libs/opencl/opencl.mk
include ${COMMON_REPO}/libs/xcl/xcl.mk

# Host Application
#host_SRCS=./src/hostMmultTest.cpp ./src/hkuCl.cpp
#host_SRCS=./src/hostv9.cpp ./src/hkuCl.cpp
host_SRCS=./src/hostv10.cpp ./src/hkuCl.cpp
host_SRCS+=$(cmdparser_SRCS) $(logger_SRCS) $(oclHelper_SRCS) $(xcl_SRCS)
host_HDRS=$(xcl_HDRS)
host_CXXFLAGS=-I./src/ -I$(XILINX_SDACCEL)/Vivado_HLS/include -std=c++0x -Wall --debug -g
host_CXXFLAGS+=-DUSE_INT
host_CXXFLAGS+=$(cmdparser_CXXFLAGS) $(logger_CXXFLAGS) $(oclHelper_CXXFLAGS) $(opencl_CXXFLAGS) $(xcl_CXXFLAGS)
host_LDFLAGS=$(opencl_LDFLAGS) 
ifeq ($(VER),2016_1)
FPOLIB=$(XILINX_SDACCEL)/Vivado_HLS/lnx64/tools/fpo_v7_0
host_LDFLAGS+=-Wl,-rpath,"$(FPOLIB)" -L $(FPOLIB) -lgmp -lmpfr -lIp_floating_point_v7_0_bitacc_cmodel
endif

EXES=host

COMMON_CLFLAGS+=-I./src/
COMMON_CLFLAGS+=--xp "prop:kernel.mmult.kernel_flags=-fno-builtin" 
COMMON_CLFLAGS+=--xp "prop:solution.hls_pre_tcl=hls_pre.tcl"

# Kernel
#mmult_SRCS=./src/krnlMmult.cpp
mmult_SRCS=./src/krnlv10.cpp
mmult_CLFLAGS=-k mmult
mmult_CLFLAGS+=$(COMMON_CLFLAGS)
mmult_CLFLAGS+=-DUSE_INT
ifeq ($(VER),2016_1)
mmult_CLFLAGS+=-DVER_2016_1
endif
mmult_CLFLAGS+=--debug

#trans_SRCS=./src/krnlTrans.cpp
#trans_CLFLAGS=-k trans
#trans_CLFLAGS+=$(COMMON_CLFLAGS)

#im2col_SRCS=./src/krnlIm2col.cpp
#im2col_CLFLAGS=-k im2col
#im2col_CLFLAGS+=$(COMMON_CLFLAGS)


#XOS=mmult trans im2col
XOS=mmult

# xclbin
#hku_XOS=trans mmult
#hku_LDCLFLAGS=-k mmult -k trans
#hku_XOS=mmult im2col
#hku_LDCLFLAGS=-k mmult -k im2col
hku_XOS=mmult 
hku_LDCLFLAGS=-k mmult

ifeq ($(VER),2016_1)
#mmult_LDCLFLAGS+=--xp "vivado_prop:run.impl.{STEPS.PHYS_OPT_DESIGN.IS_ENABLED}={true}"
#mmult_LDCLFLAGS+=--xp "vivado_prop:run.impl.{strategy}={Performance_Explore}"
#mmult_LDCLFLAGS+=--xp "vivado_prop:run.impl.{strategy}={Congestion_SpreadLogic_high}"
#mmult_LDCLFLAGS+=--xp "vivado_prop:run.impl.{strategy}={Congestion_SpreadLogic_medium}"
#mmult_LDCLFLAGS+=--xp "vivado_prop:run.impl.{strategy}={Congestion_SpreadLogic_low}"
#mmult_LDCLFLAGS+=--xp "vivado_prop:run.impl.{strategy}={Congestion_SpreadLogic_Explore}"
else ifeq ($(VER),2017_1)
#hku_LDCLFLAGS+=--kernel_frequency 140
#mmult_LDCLFLAGS+=--xp "vivado_prop:run.impl_1.{STEPS.OPT_DESIGN.ARGS.MORE OPTIONS}={-hier_fanout_limit 1000}"
#mmult_LDCLFLAGS+=--xp "vivado_prop:run.impl_1.{STEPS.PLACE_DESIGN.ARGS.MORE OPTIONS}={-fanout_opt}"
#mmult_LDCLFLAGS+=--xp "vivado_prop:run.impl_1.{STEPS.PHYS_OPT_DESIGN.IS_ENABLED}={true}"
endif
#mmult_LDCLFLAGS=--report estimate

#XCLBINS=mmult
XCLBINS=hku

# check
check_EXE=host
#check_XCLBINS=mmult
check_XCLBINS=hku

CHECKS=check

include ${COMMON_REPO}/utility/rules.mk
