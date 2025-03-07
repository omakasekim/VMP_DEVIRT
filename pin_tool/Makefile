PIN_ROOT := /home/sushikim/Project/VMP_DEVIRT/pin_tool/pin-3.25-linux
$(info PIN_ROOT is $(PIN_ROOT))

CONFIG = /home/sushikim/Project/VMP_DEVIRT/pin_tool/pin-3.25-linux/source/tools/Config/makefile.config
include $(CONFIG)

PIN_CXXFLAGS += -I$(PIN_ROOT)/source/include/pin \
                -I$(PIN_ROOT)/source/include/pin/gen \
                -I$(PIN_ROOT)/extras/xed-intel64/include/xed

APP = VMP_Trace
OBJS = VMP_Trace.o

all: dir $(APP).so

$(APP).so: $(OBJS)
	$(CXX) $(PIN_LDFLAGS) -o $@ $(OBJS) $(PIN_LIBS) $(DBG)

%.o: %.cpp
	$(CXX) $(PIN_CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so devirt.vmp.trace

