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
#include <atomic>
#include <pthread.h>
#include <cassert>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>
#include <algorithm>

struct NodeWeight {
    double weight;
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
size_t update_thres = (1 << 31);
std::string csr_file;
bool test_result = false;
size_t num_v;
size_t num_e;

std::vector<size_t*> row_ptrs;
std::vector<size_t*> col_idxs;
std::vector<volatile NodeWeight*> prs;
volatile NodeLock* global_doorbells;
std::vector<volatile NodeLock*> global_rdoorbells;
std::vector<volatile size_t*> local_doorbells;

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

void init_pagerank() {
    GetGraphStat(csr_file, &num_v, &num_e);
    row_ptrs[0] = (size_t*)AllocREGMem((num_v + 1) * sizeof(size_t));
    col_idxs[0] = (size_t*)AllocREGMem(num_e * sizeof(size_t));
    BuildCSR(csr_file, row_ptrs[0], col_idxs[0]);
    printf("node[0] built CSR\n");
}

void sync_phase(int n, int t, size_t r) {
    if (t == 0) {
        for (int tid = 0; tid < n_threads; ++tid) {
            if (tid != t) {
                WaitDoorbell(&local_doorbells[n][tid], r);
            }
        }
        
        if (n == 0) {
            for (int nid = 0; nid < n_nodes; ++nid) {
                if (nid != n) {
                    WaitDoorbell(&(global_doorbells[nid].locked), r);
                }
            }
            for (int nid = 0; nid < n_nodes; ++nid) {
                if (nid != n) {
                    SetDoorbell(&(global_rdoorbells[nid]->locked), -r);
                }
            }
        } else {
            SetDoorbell(&(global_doorbells[n].locked), r);
            WaitDoorbell(&(global_rdoorbells[n]->locked), -r);
        }

        for (int tid = 0; tid < n_threads; ++tid) {
            if (tid != t) {
                SetDoorbell(&local_doorbells[n][tid], -r);
            }
        }
    } else {
        SetDoorbell(&local_doorbells[n][t], r);
        WaitDoorbell(&local_doorbells[n][t], -r);
    }
}

void run_pagerank(int n, int t) {

    size_t v_per_node, v_per_thread, thread_v_begin, thread_v_end;

    if (t == 0) {
        if (n != 0) {
            row_ptrs[n] = (size_t*)AllocREGMem((num_v + 1) * sizeof(size_t));
            col_idxs[n] = (size_t*)AllocREGMem(num_e * sizeof(size_t));
            memcpy(row_ptrs[n], row_ptrs[0], (num_v + 1) * sizeof(size_t));
            memcpy(col_idxs[n], col_idxs[0], num_e * sizeof(size_t));
            printf("node[%d] copied CSR\n", n);
        }
        
        v_per_node = (num_v + n_nodes - 1) / n_nodes;
        prs[n] = (volatile NodeWeight*)AllocNTMem(v_per_node * sizeof(NodeWeight));
        memset((void*)(prs[n]), 0, v_per_node * sizeof(NodeWeight));
        printf("node[%d] allocated prs\n", n);
        
        local_doorbells[n] = (volatile size_t *)AllocREGMem(n_threads * sizeof(size_t));
        memset((void*)local_doorbells[n], 0, n_threads * sizeof(size_t));
        printf("node[%d] allocated local doorbells\n", n);

        if (n == 0) {
            global_doorbells = (volatile NodeLock*)AllocRELMem(n_nodes * sizeof(NodeLock));
            memset((void*)global_doorbells, 0, n_nodes * sizeof(NodeLock));
            printf("node[%d] allocated global doorbells\n", n);
        } else {
            global_rdoorbells[n] = (volatile NodeLock*)AllocRELMem(sizeof(NodeLock));
            memset((void*)global_rdoorbells[n], 0, sizeof(NodeLock));
            printf("node[%d] allocated reverse global doorbells\n", n);            
        }
    }

#ifdef NO_GEM5
        pthread_barrier_wait(&global_bar);
#else
        pthread_barrier_wait(&global_bar);
        if (warmup_rounds + n_rounds == 0) {
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
        }
#endif

    v_per_node = (num_v + n_nodes - 1) / n_nodes;
    v_per_thread = (v_per_node + n_threads - 1) / n_threads;
    thread_v_begin = v_per_node * n + v_per_thread * t;
    thread_v_end = (thread_v_begin + v_per_thread >= num_v) ? num_v : (thread_v_begin + v_per_thread);
    thread_v_end = (thread_v_begin + update_thres) < thread_v_end ? (thread_v_begin + update_thres) : thread_v_end;
    size_t* row_ptr = row_ptrs[n];
    size_t* col_idx = col_idxs[n];

    for (int r = 0; r < warmup_rounds + n_rounds; ++r) {

        if (r == warmup_rounds) {
#ifdef NO_GEM5
            pthread_barrier_wait(&global_bar);
#else
            pthread_barrier_wait(&global_bar);
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
#endif
        }

        for (size_t u = thread_v_begin; u < thread_v_end; ++u) {
            size_t out_degree = row_ptr[u + 1] - row_ptr[u];
            double self_pr = prs[u / v_per_node][u % v_per_node].weight;
            double contribution = self_pr / out_degree;
            for (size_t i = row_ptr[u]; i < row_ptr[u + 1]; ++i) {
                size_t v = col_idx[u];
                prs[v / v_per_node][v % v_per_node].weight = contribution;
            }
        }

        sync_phase(n, t, 2 * r + 1);

        for (size_t u = thread_v_begin; u < thread_v_end; ++u) {
            prs[u / v_per_node][u % v_per_node].weight = damping / num_e +
                (1 - damping) * prs[u / v_per_node][u % v_per_node].weight;
        }

        sync_phase(n, t, 2 * r + 2);
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
    while ((opt = getopt(argc, argv, "n:c:t:u:v:r:w:l:f:")) != -1) {
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
    prs.resize(n_nodes);
    local_doorbells.resize(n_nodes);
    global_rdoorbells.resize(n_nodes);

#ifdef NO_GEM5
    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
#else
    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
#endif
    pthread_barrier_init(&finish_bar, nullptr, n_nodes * n_cores);

    init_pagerank();

    std::vector<std::thread> workers;
    for (int n = 0; n < n_nodes; ++n) {
        for (int t = 0; t < n_threads; ++t) {
            if (n == 0 && t == 0)
                continue;
            workers.emplace_back(run_pagerank, n, t);
        }
        for (int t = n_threads; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }

    run_pagerank(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    printf("page rank finished\n");

    return 0;
}