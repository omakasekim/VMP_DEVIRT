# Set PIN_ROOT to the absolute path of your official Pin 3.25 kit.
PIN_ROOT := $(shell pwd)/pin-3.25-linux
$(info PIN_ROOT is $(PIN_ROOT))

# Include directories for Pin and XED headers.
PIN_CXXFLAGS += -I$(PIN_ROOT)/source/include/pin \
                -I$(PIN_ROOT)/source/include/pin/gen \
                -I$(PIN_ROOT)/extras/xed-intel64/include/xed \
                -I$(PIN_ROOT)/source/include

# Use the configuration file from the Pin kit.
CONFIG = $(PIN_ROOT)/source/tools/Config/makefile.config
include $(CONFIG)

APP = VMP_Trace
OBJS = VMP_Trace.o

all: dir $(APP).so

$(APP).so: $(OBJS)
	$(CXX) $(PIN_LDFLAGS) -o $@ $(OBJS) $(PIN_LIBS) $(DBG)

%.o: %.cpp
	$(CXX) $(PIN_CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so trace.log

# Dummy target to satisfy dependency (if required by configuration)
dir:
	@echo "Dummy target 'dir' satisfied."
