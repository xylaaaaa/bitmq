CXX ?= g++
BREW_PREFIX ?= /opt/homebrew
CXXFLAGS ?= -std=c++17 -I../include -I$(BREW_PREFIX)/include
LDFLAGS ?= -L../lib -L$(BREW_PREFIX)/lib
PROTO_LIBS := $(shell pkg-config --libs protobuf 2>/dev/null || echo -lprotobuf)
LDLIBS ?= -lmuduo_net -lmuduo_base -pthread $(PROTO_LIBS) -lz

all: client server
client:protobuf_client.cpp request.pb.cc ../include/muduo/proto/codec.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
server:protobuf_server.cpp request.pb.cc ../include/muduo/proto/codec.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)
