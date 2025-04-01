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
#include <pthread.h>
#include <cassert>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>
#include <algorithm>

struct NodeDist {
    size_t dist;
};

struct alignas(CACHELINE_SIZE) NodeLock {
    size_t locked;
    char pad[CACHELINE_SIZE - sizeof(locked)];
};
static_assert(sizeof(NodeLock) == CACHELINE_SIZE, "NodeLock size must be equal to CACHELINE_SIZE");

const double damping = 0.85;

int n_nodes = 4;
int n_cores = 1;
int n_threads = 1;
int n_rounds = 1;
int warmup_rounds = 0;
size_t v_per_lock = 128;
size_t update_thres = 4096;
size_t batch_size = 32;
std::string csr_file;
bool test_result = false;
size_t num_v;
size_t num_e;

std::vector<size_t*> row_ptrs;
std::vector<size_t*> col_idxs;
std::vector<volatile NodeDist*> dists;
std::vector<volatile NodeLock*> locks;

#ifdef NO_GEM5
pthread_barrier_t global_bar;
#else
pthread_barrier_t global_bar;
#endif
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

void init_sssp() {
    GetGraphStat(csr_file, &num_v, &num_e);
    row_ptrs[0] = (size_t*)AllocREGMem((num_v + 1) * sizeof(size_t));
    col_idxs[0] = (size_t*)AllocREGMem(num_e * sizeof(size_t));
    BuildCSR(csr_file, row_ptrs[0], col_idxs[0]);
    printf("node[0] built CSR\n");
}

void run_sssp(int n, int t) {

    size_t v_per_node, v_per_thread, l_per_node, thread_v_begin, thread_v_end;

    if (t == 0) {
        if (n != 0) {
            row_ptrs[n] = (size_t*)AllocREGMem((num_v + 1) * sizeof(size_t));
            col_idxs[n] = (size_t*)AllocREGMem(num_e * sizeof(size_t));
            memcpy(row_ptrs[n], row_ptrs[0], (num_v + 1) * sizeof(size_t));
            memcpy(col_idxs[n], col_idxs[0], num_e * sizeof(size_t));
            printf("node[%d] copied CSR\n", n);
        }
        
        v_per_node = (num_v + n_nodes - 1) / n_nodes;
        dists[n] = (volatile NodeDist*)AllocNTMem(v_per_node * sizeof(NodeDist));
        memset((void*)(dists[n]), 0, v_per_node * sizeof(NodeDist));
        printf("node[%d] allocated dists\n", n);

        l_per_node = (v_per_node + v_per_lock - 1) / v_per_lock;
        locks[n] = (volatile NodeLock*)AllocRELMem(l_per_node * sizeof(NodeLock));
        memset((void*)locks[n], 0, l_per_node * sizeof(NodeLock));
        printf("node[%d] allocated locks\n", n);
    }

#ifdef NO_GEM5
        pthread_barrier_wait(&global_bar);
#else
        pthread_barrier_wait(&global_bar);
#endif

    v_per_node = (num_v + n_nodes - 1) / n_nodes;
    v_per_thread = (v_per_node + n_threads - 1) / n_threads;
    l_per_node = (v_per_node + v_per_lock - 1) / v_per_lock;
    thread_v_begin = v_per_node * n + v_per_thread * t;
    thread_v_end = (thread_v_begin + v_per_thread >= num_v) ? num_v : (thread_v_begin + v_per_thread);
    thread_v_end = (thread_v_begin + update_thres) < thread_v_end ? (thread_v_begin + update_thres) : thread_v_end;
    size_t* row_ptr = row_ptrs[n];
    size_t* col_idx = col_idxs[n];
    size_t* update_us = (size_t*)AllocREGMem(sizeof(size_t) * batch_size);
    size_t* update_locks = (size_t*)AllocREGMem(sizeof(size_t) * batch_size * 16);  
    size_t cur_us = 0;
    size_t cur_locks = 0;

    for (int r = 0; r < warmup_rounds + n_rounds; ++r) {

        pthread_barrier_wait(&global_bar);

#ifndef NO_GEM5
        if (r == warmup_rounds) {
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
        }
#endif

        for (size_t u = thread_v_begin; u < thread_v_end; ++u) {

            size_t out_degree = row_ptr[u + 1] - row_ptr[u];
            if (out_degree == 0) {
                continue;
            }

            update_us[cur_us++] = u;
            update_locks[cur_locks++] = (u / v_per_lock);
            for (size_t i = row_ptr[u]; i < row_ptr[u + 1]; ++i) {
                update_locks[cur_locks++] = (col_idx[i] / v_per_lock);
            }

            if (cur_us == batch_size || u == thread_v_end - 1) {
                std::sort(update_locks, update_locks + cur_locks);

                size_t prev_l = -1;
                for (size_t i = 0; i < cur_locks; ++i) {
                    size_t l = update_locks[i];
                    if (l != prev_l) {
                        WaitDoorbell(&(locks[l / l_per_node][l % l_per_node].locked), 0);
                        SetDoorbell(&(locks[l / l_per_node][l % l_per_node].locked), 1);
                        prev_l = l;
                    }
                }
                
                for (size_t i = 0; i < cur_us; ++i) {
                    size_t update_u = update_us[i];
                    size_t self_dist = dists[update_u / v_per_node][update_u % v_per_node].dist;
                    for (size_t i = row_ptr[update_u]; i < row_ptr[update_u + 1]; ++i) { // each iter is a nbr of v
                        size_t v = col_idx[i];
                        size_t v_dist = dists[v / v_per_node][v % v_per_node].dist;
                        (void)v_dist;
                            dists[v / v_per_node][v % v_per_node].dist = self_dist + 1;
                    }
                }

                prev_l = -1;
                for (size_t i = 0; i < cur_locks; ++i) {
                    size_t l = update_locks[i];
                    if (l != prev_l) {
                        SetDoorbell(&(locks[l / l_per_node][l % l_per_node].locked), 0);
                        prev_l = l;
                    }
                }

                cur_us = 0;
                cur_locks = 0;
            }
        }
    }

#ifdef NO_GEM5
    pthread_barrier_wait(&global_bar);
#else
    pthread_barrier_wait(&global_bar);
    if (n == (n_nodes - 1) && t == 0) {
		appl::end_stats();
    }
#endif

    pthread_barrier_wait(&finish_bar);
}

int main(int argc, char* argv[]) {

    int opt;
    while ((opt = getopt(argc, argv, "n:c:t:u:v:r:w:l:b:f:")) != -1) {
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
            case 'u':
                update_thres = std::stoi(optarg);
                break;
            case 'v':
                test_result = std::stoi(optarg);
                break;
            case 'l':
                v_per_lock = std::stoi(optarg);
                break;
            case 'b':
                batch_size = std::stoi(optarg);
                break;
            case 'f':
                if (optarg != nullptr) {
                    csr_file = std::string(optarg);
                } else {
                    std::cerr << "Error: CSR file not specified with -f option" << std::endl;
                    return 1;
                }
                break;
            default:
                std::cerr << "Usage: " << argv[0] << "..." << std::endl;
                return 1;
        }
    }

    assert(n_threads <= n_cores);

    row_ptrs.resize(n_nodes);
    col_idxs.resize(n_nodes);
    dists.resize(n_nodes);
    locks.resize(n_nodes);

#ifdef NO_GEM5
    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
#else
    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
#endif
    pthread_barrier_init(&finish_bar, nullptr, n_nodes * n_cores);

    init_sssp();

    std::vector<std::thread> workers;
    for (int n = 0; n < n_nodes; ++n) {
        for (int t = 0; t < n_threads; ++t) {
            if (n == 0 && t == 0)
                continue;
            workers.emplace_back(run_sssp, n, t);
        }
        for (int t = n_threads; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }

    run_sssp(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    printf("sssp finished\n");

    return 0;
}