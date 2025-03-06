PIN_ROOT ?= /path/to/pin
CONFIG = $(PIN_ROOT)/source/tools/Config/makefile.config
include $(CONFIG)

APP = VMP_Trace
OBJS = VMP_Trace.o

all: $(APP).so

$(APP).so: $(OBJS)
	$(CXX) $(PIN_LDFLAGS) -o $@ $(OBJS) $(PIN_LIBS) $(DBG)

$(OBJS): VMP_Trace.cpp
	$(CXX) $(PIN_CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o *.so trace.log