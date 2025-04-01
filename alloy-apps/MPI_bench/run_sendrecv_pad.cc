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

int n_nodes = 2;
int n_cores = 1;
int n_size = 16;
int n_rounds = 1;
int warmup_rounds = 0;
int stride_in_word = 1;
int pad_reps = 0;
int pad_r = 0;
int consumer_early_quit = 0;

volatile char* data;
std::vector<volatile char*> recv_bufs;
std::vector<volatile wordT*> doorbells;

pthread_barrier_t global_bar;
pthread_barrier_t finish_bar;

void no_op(void) {
    pthread_barrier_wait(&finish_bar);
    return;
}

void run_sendrecv(int n, int t) {
    if (n == 0) {
        data = (char*)mmap(NULL, n_size * sizeof(float), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (data == MAP_FAILED) {
            std::cerr << "Error: mmap failed for data buf nid:" << n << std::endl;
            return;
        }
        memset((void*)data, 0, n_size * sizeof(float));
    } else {

        recv_bufs[n] = (char*)mmap(NULL, n_size * sizeof(float), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (recv_bufs[n] == MAP_FAILED) {
            std::cerr << "Error: mmap failed for recv buf nid:" << n << std::endl;
            return;
        }
#ifndef NO_GEM5
        appl::register_stnt((uint64_t)(recv_bufs[n]), ((uint64_t)(recv_bufs[n])) + n_size * sizeof(float));
#endif
        memset((void*)(recv_bufs[n]), 0, n_size * sizeof(float));

        doorbells[n] = (wordT*)mmap(NULL, CACHE_LINE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (doorbells[n] == MAP_FAILED) {
            std::cerr << "Error: mmap failed for doorbell nid:" << n << std::endl;
            return;
        }
#ifndef NO_GEM5
        appl::register_strel((uint64_t)(doorbells[n]), ((uint64_t)(doorbells[n])) + CACHE_LINE_SIZE);
#endif
        memset((void*)(doorbells[n]), 0, CACHE_LINE_SIZE);
    }

    volatile wordT* word_recv_buf = (volatile wordT*)(recv_bufs[n]);
    int word_recv_buf_size = n_size * sizeof(float) / sizeof(wordT);
    wordT tmp;

    int r;
    for (r = 0; r < warmup_rounds + n_rounds; ++r) {
        pthread_barrier_wait(&global_bar);
#ifndef NO_GEM5
        if (r == warmup_rounds) {
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
        }
#endif

        if (n == 0) {
            for (int nid = 1; nid < n_nodes; ++nid) {
                Send(SEND_DUMMY, recv_bufs[nid], doorbells[nid], n_size * sizeof(float), r + 1, stride_in_word);
            }
        } else {
            WaitDoorbell(doorbells[n], r + 1);
            for (int i = 0; i < word_recv_buf_size; i += stride_in_word) {
                tmp = word_recv_buf[i];
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

void run_sendrecv_pad(int n, int t) {
    if (n == 0) {
        data = (char*)mmap(NULL, n_size * sizeof(float), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (data == MAP_FAILED) {
            std::cerr << "Error: mmap failed for data buf nid:" << n << std::endl;
            return;
        }
        memset((void*)data, 0, n_size * sizeof(float));
    } else {

        recv_bufs[n] = (char*)mmap(NULL, pad_r * n_size * sizeof(float), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (recv_bufs[n] == MAP_FAILED) {
            std::cerr << "Error: mmap failed for recv buf nid:" << n << std::endl;
            return;
        }
#ifndef NO_GEM5
        appl::register_stnt((uint64_t)(recv_bufs[n]), ((uint64_t)(recv_bufs[n])) + pad_r * n_size * sizeof(float));
#endif
        memset((void*)(recv_bufs[n]), 0, pad_r * n_size * sizeof(float));

        doorbells[n] = (wordT*)mmap(NULL, pad_r * CACHE_LINE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (doorbells[n] == MAP_FAILED) {
            std::cerr << "Error: mmap failed for doorbell nid:" << n << std::endl;
            return;
        }
#ifndef NO_GEM5
        appl::register_strel((uint64_t)(doorbells[n]), ((uint64_t)(doorbells[n])) + pad_r * CACHE_LINE_SIZE);
#endif
        memset((void*)(doorbells[n]), 0, pad_r * CACHE_LINE_SIZE);
    }

    int word_recv_buf_size = n_size * sizeof(float) / sizeof(wordT);
    wordT tmp;

    int r;
    for (r = 0; r < warmup_rounds + n_rounds; ++r) {
        pthread_barrier_wait(&global_bar);
#ifndef NO_GEM5
        if (r == warmup_rounds) {
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
        }
#endif

        if (n == 0) {
            for (int i = 0; i < pad_r; ++i) {
                int recv_off = i * n_size * sizeof(float);
                int db_off = i * CACHE_LINE_SIZE / sizeof(wordT);
                for (int nid = 1; nid < n_nodes; ++nid) {
                    Send(SEND_DUMMY, recv_bufs[nid] + recv_off, doorbells[nid] + db_off, n_size * sizeof(float), r + 1, stride_in_word);
                }
            }
        } else {
            if (n == 1) {
                int db_off = (pad_r - 1) * CACHE_LINE_SIZE / sizeof(wordT);
                WaitDoorbell(doorbells[n] + db_off, r + 1);
                for (int j = 0; j < pad_r; ++j) {
                    int recv_off = j * n_size * sizeof(float);
                    volatile wordT* word_recv_buf = (volatile wordT*)(recv_bufs[n] + recv_off);
                    for (int i = 0; i < word_recv_buf_size; i += stride_in_word) {
                        tmp = word_recv_buf[i];
                    }
                }
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
    while ((opt = getopt(argc, argv, "c:r:w:f:s:n:p:")) != -1) {
        switch (opt) {
            case 'c':
                n_cores = std::stoi(optarg);
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
            case 'n':
                n_nodes = std::stoi(optarg);
                break;
            case 's':
                stride_in_word = std::stoi(optarg);
                break;
            case 'p':
                pad_reps = std::stoi(optarg);
                break;            
            default:
                std::cerr << "Usage: " << argv[0] << "..." << std::endl;
                return 1;
        }
    }
    pad_r = pad_reps / (n_nodes - 1);
    consumer_early_quit = (n_nodes > 2);

    pthread_barrier_init(&global_bar, nullptr, n_nodes);
    pthread_barrier_init(&finish_bar, nullptr, n_nodes * n_cores);

    recv_bufs.resize(n_nodes);
    doorbells.resize(n_nodes);

    std::vector<std::thread> workers;
    for (int n = 0; n < n_nodes; ++n) {
        for (int t = 0; t < 1; ++t) {
            if (n == 0 && t == 0)
                continue;
            workers.emplace_back(pad_reps ? run_sendrecv_pad : run_sendrecv, n, t);
        }
        for (int t = 1; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }
    (pad_reps ? run_sendrecv_pad : run_sendrecv)(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    printf("sendrecv finished\n");

    return 0;
}