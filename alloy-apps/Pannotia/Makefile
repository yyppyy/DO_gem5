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
PAGERANK_TARGET = build/PR
SSSP_TARGET = build/SSSP
BC_TARGET = build/BC

# Source files
PAGERANK_SRCS = PR.cc Utils.cc
SSSP_SRCS = SSSP.cc Utils.cc
BC_SRCS = BC.cc Utils.cc
HDRS = Utils.hh

# Object files
PAGERANK_OBJS = $(PAGERANK_SRCS:%.cc=build/%.o)
SSSP_OBJS = $(SSSP_SRCS:%.cc=build/%.o)
BC_OBJS = $(BC_SRCS:%.cc=build/%.o)

# Default rule to build both executables
all: build_dir $(PAGERANK_TARGET) $(SSSP_TARGET) $(BC_TARGET)

# Rule to create the build directory
build_dir:
	mkdir -p build

# Rule to link the DNN_allreduce executable
$(PAGERANK_TARGET): $(PAGERANK_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SSSP_TARGET): $(SSSP_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BC_TARGET): $(BC_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Rule to compile source files into object files for DNN_PAGERANK
build/%.o: %.cc $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up the build directory
clean:
	rm -rf build/*.o $(PAGERANK_TARGET) $(SSSP_TARGET) $(BC_TARGET)

.PHONY: all build_dir clean
