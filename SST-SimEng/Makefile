CXX=$(shell sst-config --CXX)
CXXFLAGS=$(shell sst-config --ELEMENT_CXXFLAGS)
LDFLAGS=$(shell sst-config --ELEMENT_LDFLAGS)

all: libSimEngElement.so install

libSimEngElement.so: SimEngElement.cc
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -O3 -std=c++17 -I$(SIMENG_INSTALL)/include -I$(SIMENG_BUILD)/_deps/capstone-src/include -I$(SIMENG_BUILD)/_deps/yaml-cpp-src/include -L$(SIMENG_INSTALL)/lib -lsimeng -o $@ $< Memory.cc

install:
	sst-register SimEngElement SimEngElement_LIBDIR=$(CURDIR)

clean:
	rm -f *.o libSimEngElement.so
