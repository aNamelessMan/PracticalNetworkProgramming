MUDUO_DIRECTORY ?= /usr/build/release-install-cpp11
#MUDUO_DIRECTORY ?= $(HOME)/build/install
MUDUO_INCLUDE = /usr/muduo-master
MUDUO_LIBRARY = $(MUDUO_DIRECTORY)/lib
SRC = ../src

CXXFLAGS = -g -O0 -Wall -Wextra -Werror \
	   -Wconversion -Wno-unused-parameter \
	   -Wold-style-cast -Woverloaded-virtual \
	   -Wpointer-arith -Wshadow -Wwrite-strings \
	   -march=native -rdynamic \
	   -I$(MUDUO_INCLUDE)

LDFLAGS = -L$(MUDUO_LIBRARY) -lmuduo_base -lpthread -lrt -lboost_program_options


../bin/ttcp-blocking: ttcp_blocking.cc $(MUDUO_INCLUDE)/examples/ace/ttcp/common.cc
	g++ $(CXXFLAGS) -o $@ $^ $(LDFLAGS)