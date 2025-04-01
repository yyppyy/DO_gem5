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
#include <vector>
#include <atomic>
#include <pthread.h>
#include <cassert>
#include <unistd.h>
#include <sys/mman.h>
#include <iostream>
#include <algorithm>

int n_nodes = 4;
int n_cores = 1;
int n_threads = 1;
int n_rounds = 1;
int warmup_rounds = 0;

const size_t CACHELINE_SIZE = 64;

struct alignas(CACHELINE_SIZE) NodeLock {
    size_t locked;
    char pad[CACHELINE_SIZE - sizeof(locked)];
};
static_assert(sizeof(NodeLock) == CACHELINE_SIZE, "NodeLock size must be equal to CACHELINE_SIZE");

volatile NodeLock* global_doorbells;
std::vector<volatile size_t*> global_rdoorbells;
std::vector<volatile size_t*> local_doorbells;

pthread_barrier_t global_bar;
pthread_barrier_t finish_bar;

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

void barrier(int n, int t, size_t r) {
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
                    SetDoorbell(global_rdoorbells[nid], -r);
                }
            }
        } else {
            SetDoorbell(&(global_doorbells[n].locked), r);
            WaitDoorbell(global_rdoorbells[n], -r);
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

void run_barrier(int n, int t) {

    if (t == 0) {
        
        local_doorbells[n] = (volatile size_t *)AllocREGMem(n_threads * sizeof(size_t));
        memset((void*)local_doorbells[n], 0, n_threads * sizeof(size_t));
        printf("node[%d] allocated local doorbells\n", n);

        if (n == 0) {
            global_doorbells = (volatile NodeLock*)AllocRELMem(n_nodes * sizeof(NodeLock));
            memset((void*)global_doorbells, 0, n_nodes * sizeof(NodeLock));
            printf("node[%d] allocated global doorbells\n", n);
        } else {
            global_rdoorbells[n] = (volatile size_t*)AllocRELMem(sizeof(size_t));
            memset((void*)global_rdoorbells[n], 0, sizeof(size_t));
            printf("node[%d] allocated reverse global doorbells\n", n);            
        }
    }

    for (int r = 0; r < warmup_rounds + n_rounds; ++r) {

        if (r == warmup_rounds) {
            pthread_barrier_wait(&global_bar);
#ifndef NO_GEM5
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
#endif
        }

        barrier(n, t, r + 1);
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
    while ((opt = getopt(argc, argv, "n:c:t:r:w:")) != -1) {
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
            default:
                std::cerr << "Usage: " << argv[0] << "..." << std::endl;
                return 1;
        }
    }

    assert(n_threads <= n_cores);

    local_doorbells.resize(n_nodes);
    global_rdoorbells.resize(n_nodes);

    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
    pthread_barrier_init(&finish_bar, nullptr, n_nodes * n_cores);

    std::vector<std::thread> workers;
    for (int n = 0; n < n_nodes; ++n) {
        for (int t = 0; t < n_threads; ++t) {
            if (n == 0 && t == 0)
                continue;
            workers.emplace_back(run_barrier, n, t);
        }
        for (int t = n_threads; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }

    run_barrier(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    printf("barrier finished\n");

    return 0;
}