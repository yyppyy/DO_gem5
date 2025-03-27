 # SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 # SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 #
 # NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 # property and proprietary rights in and to this material, related
 # documentation and any modifications thereto. Any use, reproduction,
 # disclosure or distribution of this material and related documentation
 # without an express license agreement from NVIDIA CORPORATION or
 # its affiliates is strictly prohibited.


# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11 -pthread -Wall -O2 -DNO_GEM5

# Target executables
ALLREDUCE_TARGET = build/run_allreduce
SCATTER_TARGET = build/run_scatter
GATHER_TARGET = build/run_gather
ALLTOALL_TARGET = build/run_alltoall
REDUCE_TARGET = build/run_reduce
SENDRECV_TARGET = build/run_sendrecv
SENDRECVPAD_TARGET = build/run_sendrecv_pad
BARRIER_TARGET = build/run_barrier

# Source files
ALLREDUCE_SRCS = run_allreduce.cc
SCATTER_SRCS = run_scatter.cc
GATHER_SRCS = run_gather.cc
ALLTOALL_SRCS = run_alltoall.cc
REDUCE_SRCS = run_reduce.cc
SENDRECV_SRCS = run_sendrecv.cc
SENDRECVPAD_SRCS = run_sendrecv_pad.cc
BARRIER_SRCS = run_barrier.cc
HDRS = Collectives.hh Utils.hh

# Object files
ALLREDUCE_OBJS = $(ALLREDUCE_SRCS:%.cc=build/%.o)
SCATTER_OBJS = $(SCATTER_SRCS:%.cc=build/%.o)
GATHER_OBJS = $(GATHER_SRCS:%.cc=build/%.o)
ALLTOALL_OBJS = $(ALLTOALL_SRCS:%.cc=build/%.o)
REDUCE_OBJS = $(REDUCE_SRCS:%.cc=build/%.o)
SENDRECV_OBJS = $(SENDRECV_SRCS:%.cc=build/%.o)
SENDRECVPAD_OBJS = $(SENDRECVPAD_SRCS:%.cc=build/%.o)
BARRIER_OBJS = $(BARRIER_SRCS:%.cc=build/%.o)

# Default rule to build both executables
all: build_dir $(ALLREDUCE_TARGET) $(SCATTER_TARGET) $(GATHER_TARGET) $(ALLTOALL_TARGET) $(REDUCE_TARGET) $(SENDRECV_TARGET) $(SENDRECVPAD_TARGET) $(BARRIER_TARGET)

# Rule to create the build directory
build_dir:
	mkdir -p build

# Rule to link the DNN_allreduce executable
$(ALLREDUCE_TARGET): $(ALLREDUCE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SCATTER_TARGET): $(SCATTER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(GATHER_TARGET): $(GATHER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(ALLTOALL_TARGET): $(ALLTOALL_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(REDUCE_TARGET): $(REDUCE_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SENDRECV_TARGET): $(SENDRECV_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SENDRECVPAD_TARGET): $(SENDRECVPAD_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BARRIER_TARGET): $(BARRIER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Rule to compile source files into object files for DNN_allreduce
build/%.o: %.cc $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up the build directory
clean:
	rm -rf build/*.o $(ALLREDUCE_TARGET) $(SCATTER_TARGET) $(GATHER_TARGET) $(ALLTOALL_TARGET) $(REDUCE_TARGET) $(SENDRECV_TARGET) $(SENDRECVPADTARGET) $(BARRIER_TARGET)

.PHONY: all build_dir clean
