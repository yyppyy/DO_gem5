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

#include "Collectives.hh"
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

int n_nodes = 4;
int n_cores = 1;
int n_threads = 1;
int n_size = 16;
int n_rounds = 1;
int warmup_rounds = 0;
int stride_in_word = 1;
bool test_result = false;
int root_nid = 0;
bool inter_round_bar = true;

std::vector<std::vector<volatile char*>> datass;
std::vector<std::vector<std::vector<volatile char*>>> recv_bufsss;
std::vector<std::vector<std::vector<volatile wordT*>>> doorbellsss;
std::vector<std::vector<std::vector<volatile wordT*>>> donesss;

pthread_barrier_t global_bar;
pthread_barrier_t finish_bar;

void no_op(void) {
    pthread_barrier_wait(&finish_bar);
    return;
}

void run_alltoall(int n, int t) {
    if (t == 0) {
        size_t data_buf_size = n_size * sizeof(float);
        size_t recv_buf_size = data_buf_size;
        size_t flag_size = CACHE_LINE_SIZE;

        datass[t][n] = (char*)mmap(NULL, data_buf_size * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (datass[t][n] == MAP_FAILED) {
            std::cerr << "Error: mmap failed for data buf nid:" << n << std::endl;
            return;
        }
        memset((void*)datass[t][n], 0, data_buf_size * n_threads);
        
        if (test_result) {
            volatile float* data_flt = reinterpret_cast<volatile float*>(datass[t][n]);
            for (int j = 0; j < n_size * n_threads; ++j) {
                data_flt[j] = 1.0 * (float)(j + 1);
            }
        }

        for (int nid = 0; nid < n_nodes; ++nid) {
            if (nid == n)
                continue;

            donesss[t][nid][n] = (wordT*)mmap(NULL, flag_size * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (donesss[t][nid][n] == MAP_FAILED) {
                std::cerr << "Error: mmap failed for done nid:" << n << std::endl;
                return;
            }
#ifndef NO_GEM5
            appl::register_strel((uint64_t)(donesss[t][nid][n]), ((uint64_t)(donesss[t][nid][n])) + flag_size * n_threads);
#endif
            memset((void*)donesss[t][nid][n], 0, flag_size * n_threads);

            recv_bufsss[t][nid][n] = (char*)mmap(NULL, recv_buf_size * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (recv_bufsss[t][nid][n] == MAP_FAILED) {
                std::cerr << "Error: mmap failed for recv buf nid:" << n << std::endl;
                return;
            }
#ifndef NO_GEM5
            appl::register_stnt((uint64_t)(recv_bufsss[t][nid][n]), ((uint64_t)(recv_bufsss[t][nid][n])) + recv_buf_size * n_threads);
#endif
            memset((void*)recv_bufsss[t][nid][n], 0, recv_buf_size * n_threads);

            doorbellsss[t][nid][n] = (wordT*)mmap(NULL, flag_size * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (doorbellsss[t][nid][n] == MAP_FAILED) {
                std::cerr << "Error: mmap failed for doorbell nid:" << n << std::endl;
                return;
            }
#ifndef NO_GEM5
            appl::register_strel((uint64_t)(doorbellsss[t][nid][n]), ((uint64_t)(doorbellsss[t][nid][n])) + flag_size * n_threads);
#endif
            memset((void*)doorbellsss[t][nid][n], 0, flag_size * n_threads);

            for (int tid = 1; tid < n_threads; ++tid) {
                datass[tid][n] = datass[0][n] + data_buf_size * tid; // redundant but fine
                donesss[tid][nid][n] = donesss[0][nid][n] + flag_size * tid / sizeof(wordT);
                recv_bufsss[tid][nid][n] = recv_bufsss[0][nid][n] + recv_buf_size * tid;
                doorbellsss[tid][nid][n] = doorbellsss[0][nid][n] + flag_size * tid / sizeof(wordT);
            }
        }
    }

    int r;
    for (r = 0; r < warmup_rounds + n_rounds; ++r) {
#ifndef NO_GEM5
        pthread_barrier_wait(&global_bar);
        if (r == warmup_rounds) {
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
        }
#endif
        AllToAllLinear<float>(datass[t],
                            recv_bufsss[t],
                            doorbellsss[t],
                            donesss[t],
                            n_size * sizeof(float),
                            n,
                            n_nodes,
                            test_result,
                            stride_in_word,
                            r + 1);
        
        if (inter_round_bar) {
            pthread_barrier_wait(&global_bar);
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
    while ((opt = getopt(argc, argv, "n:c:t:f:v:r:w:s:b:")) != -1) {
        switch (opt) {
            case 'n':
                n_nodes = std::stoi(optarg);
                root_nid = n_nodes - 1;
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
            case 'f':
                n_size = std::stoi(optarg);
                break;
            case 'v':
                test_result = std::stoi(optarg);
                break;
            case 's':
                stride_in_word = std::stoi(optarg);
                break;
            case 'b':
                inter_round_bar = std::stoi(optarg);
                break;
            default:
                std::cerr << "Usage: " << argv[0] << "..." << std::endl;
                return 1;
        }
    }

    assert(n_threads <= n_cores);
    assert((n_size * sizeof(float)) % (n_threads * CACHE_LINE_SIZE) == 0);
    n_size /= n_threads;

    datass.resize(n_threads);
    recv_bufsss.resize(n_threads);
    doorbellsss.resize(n_threads);
    donesss.resize(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        datass[t].resize(n_nodes);
        recv_bufsss[t].resize(n_nodes);
        doorbellsss[t].resize(n_nodes);
        donesss[t].resize(n_nodes);
        for (int n = 0; n < n_nodes; ++n) {
            recv_bufsss[t][n].resize(n_nodes);
            doorbellsss[t][n].resize(n_nodes);
            donesss[t][n].resize(n_nodes);
        }
    }

    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
    pthread_barrier_init(&finish_bar, nullptr, n_nodes * n_cores);

    std::vector<std::thread> workers;
    for (int n = 0; n < n_nodes; ++n) {
        for (int t = 0; t < n_threads; ++t) {
            if (n == 0 && t == 0)
                continue;
            workers.emplace_back(run_alltoall, n, t);
        }
        for (int t = n_threads; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }
    run_alltoall(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    if (test_result) {
        for (int t = 0; t < n_threads; ++t) {
            for (int sender = 0; sender < n_nodes; ++sender) {
                for (int recver = 0; recver < n_nodes; ++recver) {
                    if (sender == recver)
                        continue;
                    volatile float* recvd = reinterpret_cast<volatile float*>(recv_bufsss[t][sender][recver]);
                    volatile float* sent = reinterpret_cast<volatile float*>(datass[t][sender]);
                    for (int i = 0; i < n_size; ++i) {
                        assert(floatsEqual(sent[i], recvd[i]));
                    }
                }
            }
        }
        printf("alltoall test passed\n");
    } else {
        printf("alltoall finished\n");
    }

    return 0;
}