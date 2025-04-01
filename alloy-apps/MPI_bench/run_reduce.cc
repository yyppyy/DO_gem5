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
bool do_red_op = false;

std::vector<std::vector<volatile char*>> datass;
std::vector<std::vector<volatile char*>> correct_datass;
std::vector<std::vector<volatile char*[2]>> recv_bufss;
std::vector<std::vector<volatile wordT*[2]>> doorbellss;
std::vector<std::vector<volatile wordT*>> emptys;

#ifdef NO_GEM5
pthread_barrier_t global_bar;
#endif
pthread_barrier_t finish_bar;

void no_op(void) {
    pthread_barrier_wait(&finish_bar);
    return;
}

void run_reduce(int n, int t) {

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
            correct_datass[t][n] = new char[data_buf_size * n_threads];
            volatile float* correct_data_flt = reinterpret_cast<volatile float*>(correct_datass[t][n]);
            for (int j = 0; j < n_size * n_threads; ++j) {
                correct_data_flt[j] = 1.0 * (float)(j + 1) * n_nodes;
            }
        }

        recv_bufss[t][n][0] = (char*)mmap(NULL, recv_buf_size * 2 * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (recv_bufss[t][n][0] == MAP_FAILED) {
            std::cerr << "Error: mmap failed for recv buf nid:" << n << std::endl;
            return;
        }
        recv_bufss[t][n][1] = recv_bufss[t][n][0] + recv_buf_size;
    #ifndef NO_GEM5
        appl::register_stnt((uint64_t)(recv_bufss[t][n][0]), ((uint64_t)(recv_bufss[t][n][0])) + recv_buf_size * 2 * n_threads);
    #endif
        memset((void*)recv_bufss[t][n][0], 0, recv_buf_size * 2 * n_threads);


        doorbellss[t][n][0] = (wordT*)mmap(NULL, flag_size * 2 * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (doorbellss[t][n][0] == MAP_FAILED) {
            std::cerr << "Error: mmap failed for doorbell nid:" << n << std::endl;
            return;
        }
        doorbellss[t][n][1] = doorbellss[t][n][0] + flag_size / sizeof(wordT);
    #ifndef NO_GEM5
        appl::register_strel((uint64_t)(doorbellss[t][n][0]), ((uint64_t)(doorbellss[t][n][0])) + flag_size * 2 * n_threads);
    #endif
        memset((void*)doorbellss[t][n][0], 0, flag_size * 2 * n_threads);


        emptys[t][n] = (wordT*)mmap(NULL, flag_size * n_threads, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (emptys[t][n] == MAP_FAILED) {
            std::cerr << "Error: mmap failed for empty nid:" << n << std::endl;
            return;
        }
    #ifndef NO_GEM5
        appl::register_strel((uint64_t)(emptys[t][n]), ((uint64_t)(emptys[t][n])) + flag_size * n_threads);
    #endif
        memset((void*)emptys[t][n], 0, flag_size * n_threads);


        for (int tid = 1; tid < n_threads; ++tid) {
            datass[tid][n] = datass[t][n] + data_buf_size * tid;
            if (test_result)
                correct_datass[tid][n] = correct_datass[t][n] + data_buf_size * tid;
            recv_bufss[tid][n][0] = recv_bufss[t][n][0] + recv_buf_size * tid * 2;
            recv_bufss[tid][n][1] = recv_bufss[t][n][0] + recv_buf_size * (tid * 2 + 1);
            doorbellss[tid][n][0] = doorbellss[t][n][0] + flag_size * tid * 2 / sizeof(wordT);
            doorbellss[tid][n][1] = doorbellss[t][n][0] + flag_size * (tid * 2 + 1) / sizeof(wordT);
            emptys[tid][n] = emptys[t][n] + flag_size * tid / sizeof(wordT);
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
        ReduceBinaryTree<float>(datass[t],
                            recv_bufss[t],
                            doorbellss[t],
                            emptys[t],
                            n_size * sizeof(float),
                            0,
                            n,
                            n_nodes,
                            do_red_op,
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
                do_red_op = test_result;
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
    if (test_result)
        correct_datass.resize(n_threads);
    recv_bufss.resize(n_threads);
    doorbellss.resize(n_threads);
    emptys.resize(n_threads);
    for (int t = 0; t < n_threads; ++t) {
        datass[t] = std::vector<volatile char*>(n_nodes);
        if (test_result)
            correct_datass[t] = std::vector<volatile char*>(n_nodes);
        recv_bufss[t] = std::vector<volatile char*[2]>(n_nodes);
        doorbellss[t] = std::vector<volatile wordT*[2]>(n_nodes);
        emptys[t] = std::vector<volatile wordT*>(n_nodes);
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
            workers.emplace_back(run_reduce, n, t);
        }
        for (int t = n_threads; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }
    run_reduce(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    if (test_result) {
        for (int t = 0; t < n_threads; ++t) {
            volatile float* data = reinterpret_cast<volatile float*>(datass[t][0]);
            volatile float* correct_data = reinterpret_cast<volatile float*>(correct_datass[t][0]);
            for (int i = 0; i < n_size; ++i) {
                printf("t[%d] i[%d] d[%f] c[%f]\n", t, i, data[i], correct_data[i]);
                assert(floatsEqual(data[i], correct_data[i]));
            }
        }
        printf("reduce test passed\n");
    } else {
        printf("reduce finished\n");
    }

    return 0;
}