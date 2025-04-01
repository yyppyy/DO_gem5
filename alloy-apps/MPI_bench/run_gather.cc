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

std::vector<std::vector<volatile char*>> datass;
std::vector<std::vector<volatile char*>> recv_bufss;
std::vector<std::vector<volatile wordT*>> doorbellss;
std::vector<std::vector<volatile wordT*>> doness;

#ifdef NO_GEM5
pthread_barrier_t global_bar;
#endif
pthread_barrier_t finish_bar;

void no_op(void) {
    pthread_barrier_wait(&finish_bar);
    return;
}

void run_gather(int n, int t) {
    if (t == 0) {
        size_t data_buf_size = n_size * sizeof(float);
        size_t recv_buf_size = data_buf_size;
        size_t flag_size = CACHE_LINE_SIZE;

        if (n == root_nid) {
            for (int nid = 0; nid < n_nodes; ++nid) {
                if (nid == root_nid)
                    continue;
                
                recv_bufss[t][nid] = (char*)mmap(NULL, recv_buf_size * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (recv_bufss[t][nid] == MAP_FAILED) {
                    std::cerr << "Error: mmap failed for recv buf nid:" << n << std::endl;
                    return;
                }
#ifndef NO_GEM5
                appl::register_stnt((uint64_t)(recv_bufss[t][nid]), ((uint64_t)(recv_bufss[t][nid])) + recv_buf_size * n_threads);
#endif
                memset((void*)recv_bufss[t][nid], 0, recv_buf_size * n_threads);

                doorbellss[t][nid] = (wordT*)mmap(NULL, flag_size * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
                if (doorbellss[t][nid] == MAP_FAILED) {
                    std::cerr << "Error: mmap failed for doorbell nid:" << n << std::endl;
                    return;
                }
#ifndef NO_GEM5
                appl::register_strel((uint64_t)(doorbellss[t][nid]), ((uint64_t)(doorbellss[t][nid])) + flag_size * n_threads);
#endif
                memset((void*)doorbellss[t][nid], 0, flag_size * n_threads);

                for (int tid = 1; tid < n_threads; ++tid) {
                    recv_bufss[tid][nid] = recv_bufss[0][nid] + recv_buf_size * tid;
                    doorbellss[tid][nid] = doorbellss[0][nid] + flag_size * tid / sizeof(wordT);
                }
            }
            
        } else {

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

            doness[t][n] = (wordT*)mmap(NULL, flag_size * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (doness[t][n] == MAP_FAILED) {
                std::cerr << "Error: mmap failed for done nid:" << n << std::endl;
                return;
            }
#ifndef NO_GEM5
            appl::register_strel((uint64_t)(doness[t][n]), ((uint64_t)(doness[t][n])) + flag_size * n_threads);
#endif
            memset((void*)doness[t][n], 0, flag_size * n_threads);

            for (int tid = 0; tid < n_threads; ++tid) {
                datass[tid][n] = datass[0][n] + data_buf_size * tid;
                doness[tid][n] = doness[0][n] + flag_size * tid / sizeof(wordT);
            }
        }
    }

    int r;
    for (r = 0; r < warmup_rounds + n_rounds; ++r) {
#ifdef NO_GEM5
        pthread_barrier_wait(&global_bar);
#else
        appl::gem5_barrier_wait(r);
        if (r == warmup_rounds) {
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
        }
#endif
        GatherLinear<float>(datass[t],
                            recv_bufss[t],
                            doorbellss[t],
                            doness[t],
                            n_size * sizeof(float),
                            root_nid,
                            n,
                            n_nodes,
                            test_result,
                            stride_in_word,
                            r + 1);
    }

#ifdef NO_GEM5
    pthread_barrier_wait(&global_bar);
#else
    appl::gem5_barrier_wait(r);
    if (n == (n_nodes - 1) && t == 0) {
		appl::end_stats();
    }
#endif

    pthread_barrier_wait(&finish_bar);
}

int main(int argc, char* argv[]) {

    int opt;
    while ((opt = getopt(argc, argv, "n:c:t:f:v:r:w:s:")) != -1) {
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
            default:
                std::cerr << "Usage: " << argv[0] << "..." << std::endl;
                return 1;
        }
    }

    assert(n_threads <= n_cores);
    assert((n_size * sizeof(float)) % (n_threads * CACHE_LINE_SIZE) == 0);
    n_size /= n_threads;

    datass.resize(n_threads);
    recv_bufss.resize(n_threads);
    doorbellss.resize(n_threads);
    doness.resize(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        datass[t] = std::vector<volatile char*>(n_nodes);
        recv_bufss[t] = std::vector<volatile char*>(n_nodes);
        doorbellss[t] = std::vector<volatile wordT*>(n_nodes);
        doness[t] = std::vector<volatile wordT*>(n_nodes);
    }

#ifdef NO_GEM5
    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
#else
    appl::gem5_barrier_init(n_nodes * n_threads);
#endif
    pthread_barrier_init(&finish_bar, nullptr, n_nodes * n_cores);

    std::vector<std::thread> workers;
    for (int n = 0; n < n_nodes; ++n) {
        for (int t = 0; t < n_threads; ++t) {
            if (n == 0 && t == 0)
                continue;
            workers.emplace_back(run_gather, n, t);
        }
        for (int t = n_threads; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }
    run_gather(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    if (test_result) {
        for (int t = 0; t < n_threads; ++t) {
            for (int n = 0; n < n_nodes; ++n) {
                if (n == root_nid)
                    continue;
                volatile float* recvd = reinterpret_cast<volatile float*>(recv_bufss[t][n]);
                volatile float* sent = reinterpret_cast<volatile float*>(datass[t][n]);
                for (int i = 0; i < n_size; ++i) {
                    assert(floatsEqual(sent[i], recvd[i]));
                }
            }
        }
        printf("gather test passed\n");
    } else {
        printf("gather finished\n");
    }

    return 0;
}