/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
 * AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

#include "Utils.hh"
#ifndef NO_GEM5
#include "appl.h"
#endif

#include <iostream>
#include <fstream>
#include <sstream>
#include <cassert>
#include <sys/mman.h>

void* AllocREGMem(size_t count) {
    void* ret = mmap(NULL, count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "Error: mmap" << std::endl;
        return nullptr;
    }
    return ret;
}

void* AllocNTMem(size_t count) {
    void* ret = mmap(NULL, count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "Error: mmap" << std::endl;
        return nullptr;
    }
#ifndef NO_GEM5
    appl::register_stnt((uint64_t)ret, (uint64_t)ret + count);
#endif
    return ret;
}

void* AllocRELMem(size_t count) {
    void* ret = mmap(NULL, count, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ret == MAP_FAILED) {
        std::cerr << "Error: mmap" << std::endl;
        return nullptr;
    }
#ifndef NO_GEM5
    appl::register_strel((uint64_t)ret, (uint64_t)ret + count);
#endif
    return ret;
}

void BuildCSR(const std::string filename,
              size_t* row_ptr,
              size_t* col_idx) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::string line;
    int l_idx = 0;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        if (l_idx == 0) {
            ;
        } else if (l_idx == 1) {
            ;
        } else if (l_idx == 2) {
            size_t value;
            int i = 0;
            while (iss >> value) {
                row_ptr[i++] = value;
                if (i % 1000 == 0)
                    std::cout << "load row_ptrs:" << i << std::endl;
            }
        } else if (l_idx == 3) {
            size_t value;
            int i = 0;
            while (iss >> value) {
                col_idx[i++] = value;
                if (i % 1000 == 0)
                    std::cout << "load col_idxs:" << i << std::endl;
            }
        } else {
            assert(0);
        }
        ++l_idx;
    }

    file.close();
}

void GetGraphStat(const std::string filename,
              size_t* num_v,
              size_t* num_e) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << std::endl;
        return;
    }

    std::string line;
    int l_idx = 0;
    while (std::getline(file, line)) {
        std::istringstream iss(line);

        if (l_idx == 0) {
            if (num_v)
                *num_v = atoi(line.c_str()) - 1;
            std::cout << "get num_v:" << *num_v << std::endl;
        } else if (l_idx == 1) {
            if (num_e)
                *num_e = atoi(line.c_str());
            std::cout << "get num_e:" << *num_e << std::endl;
        } else break;
        ++l_idx;
    }

    file.close();
}