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
size_t m2       = 8;
size_t d_per_node     = 1;
const size_t DATA_PER_ELEM = 64;

struct alignas(CACHELINE_SIZE) element_t {
    size_t data[DATA_PER_ELEM];
};
static_assert(sizeof(element_t) == DATA_PER_ELEM * sizeof(size_t), "element size must be equal to CACHELINE_SIZE");

struct alignas(CACHELINE_SIZE) flag_t {
    size_t done;
    char pad[CACHELINE_SIZE - sizeof(done)];
};
static_assert(sizeof(flag_t) == CACHELINE_SIZE, "flag size must be equal to CACHELINE_SIZE");

std::vector<volatile element_t*> matrix;
std::vector<volatile flag_t*> flag;

pthread_barrier_t global_bar;
pthread_barrier_t finish_bar;

void no_op(void) {
    pthread_barrier_wait(&finish_bar);
    return;
}

inline void WaitDoorbell(volatile size_t* doorbell, size_t doorbellVal) {
    while (*doorbell != doorbellVal);
}

inline bool CheckDoorbell(volatile size_t* doorbell, size_t doorbellVal) {
    return *doorbell == doorbellVal;
}

inline void SetDoorbell(volatile size_t* doorbell, size_t doorbellVal) {
    *doorbell = doorbellVal;
}

void init_trns() {

}

void run_trns(int n, int t) {

    if (t == 0) {
        matrix[n] = (volatile element_t*)AllocNTMem(d_per_node * sizeof(element_t));
        memset((void*)matrix[n], 0, d_per_node * sizeof(element_t));
        printf("node[%d] allocated matrix\n", n);

        flag[n] = (volatile flag_t*)AllocRELMem(d_per_node * sizeof(flag_t));
        memset((void*)matrix[n], 0, d_per_node * sizeof(flag_t));
        printf("node[%d] allocated flag\n", n);
    }

    bool *mvmp = (bool *)AllocREGMem(m1 * m2 * sizeof(bool));
        
    pthread_barrier_wait(&global_bar);
#ifndef NO_GEM5
    if (warmup_rounds + n_rounds == 0) {
        if (n == (n_nodes - 1) && t == 0) {
            appl::start_stats();
        }
    }
#endif

    for (int r = 0; r < warmup_rounds + n_rounds; ++r) {

        memset(mvmp, 0, m1 * m2 * sizeof(bool));

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
        
        size_t row = n * n_threads + t;
        size_t col = 0;
        size_t moved = 0;

        while (moved < m1 * m2) {
            size_t cur_pos = row * m2 + col;
            size_t cur_i = cur_pos / d_per_node;
            size_t cur_j = cur_pos % d_per_node;
            if (!mvmp[cur_pos]) {
                if (!CheckDoorbell(&(flag[cur_i][cur_j].done), 1)) {
                    size_t nxt_pos = col * m1 + row;
                    size_t nxt_i = nxt_pos / d_per_node;
                    size_t nxt_j = nxt_pos % d_per_node;
                    for (size_t i = 0; i < DATA_PER_ELEM; i += stride) {
                        matrix[nxt_i][nxt_j].data[i] = matrix[cur_i][cur_j].data[i];
                    }
                    SetDoorbell(&(flag[cur_i][cur_j].done), 1);
                    
                    row = nxt_pos / m2;
                    col = nxt_pos % m2;
                    assert(row < m1);
                    assert(col < m2);
                } else {
                    size_t nxt_pos = (cur_pos + 1) % (m1 * m2);
                    row = nxt_pos / m2;
                    col = nxt_pos % m2;
                }
                mvmp[cur_pos] = 1;
                ++moved;
            } else {
                size_t nxt_pos = (cur_pos + 1) % (m1 * m2);
                row = nxt_pos / m2;
                col = nxt_pos % m2;
            }
        }
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
    while ((opt = getopt(argc, argv, "n:c:t:v:r:w:m:")) != -1) {
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
            case 'm':
                m2 = std::stoi(optarg);
                break;
            case 'v':
                test_result = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << "..." << std::endl;
                return 1;
        }
    }
    m1 = (n_nodes * n_threads);
    d_per_node = m1 * m2 / n_nodes;

    assert(n_threads <= n_cores);
    assert(m1 * m2 % n_nodes == 0);

    matrix.resize(n_nodes);
    flag.resize(n_nodes);

    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
    pthread_barrier_init(&finish_bar, nullptr, n_nodes * n_cores);

    init_trns();

    std::vector<std::thread> workers;
    for (int n = 0; n < n_nodes; ++n) {
        for (int t = 0; t < n_threads; ++t) {
            if (n == 0 && t == 0)
                continue;
            workers.emplace_back(run_trns, n, t);
        }
        for (int t = n_threads; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }

    run_trns(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    printf("trns finished\n");

    return 0;
}