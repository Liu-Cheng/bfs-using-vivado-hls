# You must configure ALTERAOCLSDKROOT to point the root directory of the Altera SDK for OpenCL
# software installation.
# See http://www.altera.com/literature/hb/opencl-sdk/aocl_getting_started.pdf 
# for more information on installing and configuring the Altera SDK for OpenCL.

ifeq ($(VERBOSE),1)
ECHO := 
else
ECHO := @
endif

TARGET := host

# Where is the Altera SDK for OpenCL software?
ifeq ($(wildcard $(ALTERAOCLSDKROOT)),)
$(error Set ALTERAOCLSDKROOT to the root directory of the Altera SDK for OpenCL software installation)
endif
ifeq ($(wildcard $(ALTERAOCLSDKROOT)/host/include/CL/opencl.h),)
$(error Set ALTERAOCLSDKROOT to the root directory of the Altera SDK for OpenCL software installation.)
endif

# Directories.
TARGET_DIR := bin
INC_DIRS := host/inc ../common/inc ../extlibs/inc 

# All include files.
INC_FILES := $(foreach D,$(INC_DIRS),$(wildcard $D/*.h))

# Source files.
SRCS := $(wildcard ../common/src/AOCLUtils/*.cpp) $(wildcard src/*.cpp)

# Libraries.
LIBS :=

# Compiler.
CXX := g++

# OpenCL compile and link flags.
AOCL_COMPILE_CONFIG := $(shell aocl compile-config)
AOCL_LINK_CONFIG := $(shell aocl link-config)

# Compilation flags
ifeq ($(DEBUG),1)
CXXFLAGS += -g -D__USE_XOPEN2K8
else
CXXFLAGS += -O2 -D__USE_XOPEN2K8
endif
CXXFLAGS+= -Wall -I${AALSDK}/include -I/usr/local/include -DHAVE_CONFIG_H -DTESTB -g -L${AALSDK}/lib  -L/usr/local/lib -L/home/xinyuc/Opt/libtool/lib

CPPFLAGS += -std=c++0x
#CPPFLAGS += -std=c++11 -std=c++0x
# Make it all!
all : $(TARGET_DIR)/$(TARGET)

# Host executable target.
$(TARGET_DIR)/$(TARGET) : Makefile $(SRCS) $(INC_FILES)
	@[ -d $(TARGET_DIR) ] || mkdir $(TARGET_DIR)
	$(ECHO)$(CXX) $(CPPFLAGS) $(CXXFLAGS) -fPIC $(foreach D,$(INC_DIRS),-I$D) \
			$(AOCL_COMPILE_CONFIG) $(SRCS) $(AOCL_LINK_CONFIG) \
			$(foreach D,$(LIB_DIRS),-L$D) \
			$(foreach L,$(LIBS),-l$L) \
			-o $(TARGET_DIR)/$(TARGET)
# Standard make targets
clean :
	$(ECHO)rm -f $(TARGET_DIR)/$(TARGET)
copy :
	cp bin/host sim/
.PHONY : all clean

