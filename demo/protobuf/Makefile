CXX ?= g++
BREW_PREFIX ?= /opt/homebrew
CXXFLAGS ?= -std=c++17 -I$(BREW_PREFIX)/include
LDFLAGS ?= -L$(BREW_PREFIX)/lib
PROTO_LIBS := $(shell pkg-config --libs protobuf 2>/dev/null || echo -lprotobuf)
LDLIBS ?= $(PROTO_LIBS)

main: main.cc contacts.pb.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
