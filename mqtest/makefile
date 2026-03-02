CXX ?= clang++
BREW_PREFIX ?= /opt/homebrew
CXXFLAGS ?= -g -std=c++17 -I$(BREW_PREFIX)/include -I../demo/muduo/include
LDFLAGS ?= -L$(BREW_PREFIX)/lib
PROTO_LIBS := $(shell pkg-config --libs protobuf)
GTEST_LIBS := $(shell pkg-config --libs gtest 2>/dev/null || echo -lgtest)
LDLIBS_TEST ?= $(GTEST_LIBS) $(PROTO_LIBS) -lsqlite3 -pthread
LDLIBS_FILE ?= -lsqlite3

PROTO_SRCS = ../mqcommon/mq_msg.pb.cc ../mqcommon/mq_proto.pb.cc
TEST_BINS = mq_msgtest mq_bindingtest mq_queuetest mq_filetest mq_exchangetest mq_routetest mq_hosttest mq_consumertest

.PHONY: all clean
all: $(TEST_BINS)

mq_connectiontest: mq_connectiontest.cpp $(PROTO_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_TEST)

mq_channeltest: mq_channeltest.cpp $(PROTO_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_TEST)

mq_msgtest: mq_msgtest.cpp ../mqcommon/mq_msg.pb.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_TEST)

mq_bindingtest: mq_bindingtest.cpp $(PROTO_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_TEST)

mq_queuetest: mq_queuetest.cpp $(PROTO_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_TEST)

mq_filetest: mq_filetest.cpp
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_FILE)

mq_exchangetest: mq_exchangetest.cpp $(PROTO_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_TEST)

mq_routetest: mq_routetest.cpp ../mqcommon/mq_msg.pb.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_TEST)

mq_hosttest: mq_hosttest.cpp $(PROTO_SRCS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_TEST)

mq_consumertest: mq_consumertest.cpp ../mqcommon/mq_msg.pb.cc
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS_TEST)

clean:
	rm -f $(TEST_BINS)
