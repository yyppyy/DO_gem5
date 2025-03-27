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
TQH_TARGET = build/TQH
HSTI_TARGET = build/HSTI
TRNS_TARGET = build/TRNS
PAD_TARGET = build/PAD

# Source files
TQH_SRCS = TQH.cc Utils.cc
HSTI_SRCS = HSTI.cc Utils.cc
TRNS_SRCS = TRNS.cc Utils.cc
PAD_SRCS = PAD.cc Utils.cc
HDRS = Utils.hh

# Object files
TQH_OBJS = $(TQH_SRCS:%.cc=build/%.o)
HSTI_OBJS = $(HSTI_SRCS:%.cc=build/%.o)
TRNS_OBJS = $(TRNS_SRCS:%.cc=build/%.o)
PAD_OBJS = $(PAD_SRCS:%.cc=build/%.o)

# Default rule to build both executables
all: build_dir $(TQH_TARGET) $(PAD_TARGET) $(HSTI_TARGET) $(TRNS_TARGET)

# Rule to create the build directory
build_dir:
	mkdir -p build

# Rule to link the DNN_allreduce executable
$(TQH_TARGET): $(TQH_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(HSTI_TARGET): $(HSTI_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TRNS_TARGET): $(TRNS_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(PAD_TARGET): $(PAD_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Rule to compile source files into object files for DNN_TQH
build/%.o: %.cc $(HDRS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up the build directory
clean:
	rm -rf build/*.o $(TQH_TARGET) $(PAD_TARGET) $(HSTI_TARGET) $(TRNS_TARGET)

.PHONY: all build_dir clean
