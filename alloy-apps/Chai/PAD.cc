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

#include <thread>
#include <cstring>
#include <string>
#include <atomic>
#include <pthread.h>
#include <cassert>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#define SIM_COALEASE

int n_nodes = 4;
int n_cores = 1;
int n_threads = 1;
int n_rounds = 1;
int warmup_rounds = 0;
bool test_result = false;

size_t m1             = 1024;
size_t m2             = 1023;
size_t pad            = 1;
size_t d_per_node     = 1;

std::vector<volatile size_t*> matrix;
std::vector<volatile size_t*> gdbs;
std::vector<volatile size_t*> ldbs;

pthread_barrier_t global_bar;
pthread_barrier_t finish_bar;

void no_op(void) {
    pthread_barrier_wait(&finish_bar);
    return;
}

inline void WaitDoorbell(volatile size_t* doorbell, size_t doorbellVal) {
    while (*doorbell != doorbellVal);
}

inline void SetDoorbell(volatile size_t* doorbell, size_t doorbellVal) {
    *doorbell = doorbellVal;
}

void init_pad() {

}

void sync_wg(int n, int t, size_t r) {
    if (t == 0) {
        for (int tid = 1; tid < n_threads; ++tid) {
            WaitDoorbell(&ldbs[n][tid], r);
        }

        if (n > 0) {
            SetDoorbell(gdbs[n - 1], r);
        }
        if (n < n_nodes - 1) {
            WaitDoorbell(gdbs[n], r);
        }

        for (int tid = 1; tid < n_threads; ++tid) {
            SetDoorbell(&ldbs[n][tid], -r);
        }
    } else {
        SetDoorbell(&ldbs[n][t], r);
        WaitDoorbell(&ldbs[n][t], -r);
    }
}

void run_pad(int n, int t) {

    if (t == 0) {
        matrix[n] = (volatile size_t*)AllocNTMem(d_per_node * sizeof(size_t));
        memset((void*)matrix[n], 0, d_per_node * sizeof(size_t));
        printf("node[%d] allocated matrix\n", n);

        gdbs[n] = (volatile size_t*)AllocRELMem(sizeof(size_t));
        memset((void*)gdbs[n], 0, sizeof(size_t));
        printf("node[%d] allocated global doorbells\n", n);

        ldbs[n] = (volatile size_t *)AllocREGMem(n_threads * sizeof(size_t));
        memset((void*)ldbs[n], 0, n_threads * sizeof(size_t));
        printf("node[%d] allocated local doorbells\n", n);      

    }
        
    pthread_barrier_wait(&global_bar);
#ifndef NO_GEM5
    if (warmup_rounds + n_rounds == 0) {
        if (n == (n_nodes - 1) && t == 0) {
            appl::start_stats();
        }
    }
#endif

    for (int r = 0; r < warmup_rounds + n_rounds; ++r) {

        if (r == warmup_rounds) {
            pthread_barrier_wait(&global_bar);
#ifndef NO_GEM5
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
#endif
        }

#ifdef SIM_COALEASE
        const int stride = 8;
#else
        const int stride = 1;
#endif

        size_t out_row = n * n_threads + t;
        size_t in_pos = out_row * m2;
        size_t out_pos = out_row * (m2 + pad);
        size_t out_col;
        for (out_col = 0; out_col < m2; out_col += stride) {
            matrix[out_pos / d_per_node][out_pos % d_per_node] = matrix[in_pos / d_per_node][in_pos % d_per_node];
            in_pos += stride;
            out_pos += stride;
        }
        for (; out_col < m2 + pad; out_col += stride) {
            matrix[out_pos / d_per_node][out_pos % d_per_node] = 0;
            out_pos += stride;
        }

        sync_wg(n, t, 2 * r + 1);
    }

    pthread_barrier_wait(&global_bar);
#ifndef NO_GEM5
    if (n == (n_nodes - 1) && t == 0) {
		appl::end_stats();
    }
#endif

    pthread_barrier_wait(&finish_bar);
}

int main(int argc, char* argv[]) {

    int opt;
    while ((opt = getopt(argc, argv, "n:c:t:v:r:w:p:")) != -1) {
        switch (opt) {
            case 'n':
                n_nodes = std::stoi(optarg);
                break;
            case 'c':
                n_cores = std::stoi(optarg);
                break;
            case 't':
                n_threads = std::stoi(optarg);
                break;
            case 'r':
                n_rounds = std::stoi(optarg);
                break;
            case 'w':
                warmup_rounds = std::stoi(optarg);
                break;
            case 'v':
                test_result = std::stoi(optarg);
                break;
            case 'p':
                pad = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << "..." << std::endl;
                return 1;
        }
    }
    m1 = n_nodes * n_threads;
    d_per_node = (m1 * (m2 + pad)) / n_nodes;

    assert(n_threads <= n_cores);
    assert((m1 * (m2 + pad)) % n_nodes == 0);

    matrix.resize(n_nodes);
    gdbs.resize(n_nodes);
    ldbs.resize(n_nodes);

    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
    pthread_barrier_init(&finish_bar, nullptr, n_nodes * n_cores);

    init_pad();

    std::vector<std::thread> workers;
    for (int n = 0; n < n_nodes; ++n) {
        for (int t = 0; t < n_threads; ++t) {
            if (n == 0 && t == 0)
                continue;
            workers.emplace_back(run_pad, n, t);
        }
        for (int t = n_threads; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }

    run_pad(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    printf("pad finished\n");

    return 0;
}