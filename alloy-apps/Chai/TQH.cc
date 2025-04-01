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

struct Op {
    size_t code;
    Op (size_t _code) {
        code = _code;
    }
};

struct alignas(CACHELINE_SIZE) NodeLock {
    size_t locked;
    char pad[CACHELINE_SIZE - sizeof(locked)];
};
static_assert(sizeof(NodeLock) == CACHELINE_SIZE, "NodeLock size must be equal to CACHELINE_SIZE");

int n_nodes = 4;
int n_cores = 1;
int n_threads = 1;
int n_rounds = 1;
int warmup_rounds = 0;
std::string input_file;
bool test_result = false;
size_t pool_size     = 3200;
size_t queue_size    = 320;
size_t m             = 288;
size_t n             = 352;
size_t n_bins        = 256;
size_t hist_size     = 1000;
size_t frame_size = 100;
size_t threads_per_frame = 32;

std::vector<size_t*> datas;
std::vector<volatile size_t*> hists;
std::vector<size_t*> local_hists;

std::vector<std::vector<volatile Op*>> task_queuess;
std::vector<std::vector<volatile size_t*>> headss;
std::vector<std::vector<volatile size_t*>> tailss;

pthread_barrier_t global_bar;
pthread_barrier_t finish_bar;

void read_input() {
    int fr = 0;
    char dctFileName[2][100];
    std::ifstream file[2];
    float v;

    datas[0] = (size_t*)AllocREGMem(pool_size * frame_size * sizeof(size_t));

    sprintf(dctFileName[0], "%s%d.float", input_file.c_str(), 0);
    sprintf(dctFileName[1], "%s%d.float", input_file.c_str(), 1);

    file[0].open(dctFileName[0]);
    file[1].open(dctFileName[1]);

    if (!file[0].is_open() || !file[1].is_open()) {
        std::cerr << "Unable to open files " << dctFileName[0] << " or " << dctFileName[1] << std::endl;
        exit(-1);
    }

    for (int i = 0; i < pool_size; i++) {
        std::ifstream& currentFile = file[i % 2];
        
        currentFile.clear(); // Clear any eof or fail flags
        currentFile.seekg(0, std::ios::beg);

        if (i == 0) {
            std::string line;
            float tmp;
            for (int y = 0; y < m; ++y) {
                std::getline(currentFile, line);
                std::istringstream iss(line);
                for (int x = 0; x < n; ++x) {
                    iss >> tmp;
                    datas[0][i * frame_size + y * n + x] = (size_t)tmp;
                }
            }
        } else {
            for (int y = 0; y < m; ++y) {
                for (int x = 0; x < n; ++x) {
                    datas[0][i * frame_size + y * n + x] = datas[0][y * n + x];
                }
            }
        }
        printf("frame[%d] loaded\n", fr);
        fr++;
    }

    file[0].close();
    file[1].close();
}

void no_op(void) {
    pthread_barrier_wait(&finish_bar);
    return;
}

size_t calc_avail_size(size_t head, size_t tail) {
    if (head == tail) {
        return queue_size - 1;
    } else if (head > tail) {
        return head - tail - 1;
    } else {
        return queue_size + head - tail - 1;
    }
}

size_t waitPush(size_t cur_tail, volatile size_t* head) {
    size_t cur_head, avail_size;
    do {
        cur_head = *head;
        avail_size = calc_avail_size(cur_head, cur_tail);
    } while (avail_size < 1);
    return (cur_tail + 1) % queue_size;
}

void setTail(size_t next_tail, volatile size_t* tail) {
    *tail = next_tail;
}

size_t waitPop(size_t cur_head, volatile size_t* tail) {
    size_t cur_tail, used;
    do {
        cur_tail = *tail;
        used = queue_size - 1 - calc_avail_size(cur_head, cur_tail);
    } while (used < 1);
    return (cur_head + 1) % queue_size;
}

void setHead(size_t next_head, volatile size_t* head) {
    *head = next_head;
}

void insertQueue(size_t cur_tail, volatile Op* task_queue) {
    size_t DUMMY = 1;
    task_queue[cur_tail].code = DUMMY;
}

Op removeQueue(size_t cur_head, volatile Op* task_queue) {
    return Op{task_queue[cur_head].code};
}

void init_tqh() {
    read_input();
    printf("node[0] read frames\n");
}

void run_tqh(int n, int t) {

    if (t == 0) {
        if (n != 0) {
            datas[n] = (size_t*)AllocREGMem(pool_size * frame_size * sizeof(size_t));
            memcpy((void*)datas[n], datas[0], pool_size * frame_size * sizeof(size_t));
            printf("node[%d] copied frames\n", n);
        }
        hists[n] = (volatile size_t*)AllocNTMem(hist_size * sizeof(size_t) / n_nodes);
        memset((void*)hists[n], 0, hist_size * sizeof(size_t) / n_nodes);
        printf("node[%d] allocated hist\n", n);
        local_hists[n] = (size_t*)AllocNTMem(n_bins * sizeof(size_t));
        memset((void*)local_hists[n], 0, n_bins * sizeof(size_t));
        printf("node[%d] allocated local hist\n", n);
    }

    if (n == 0) {
        for (int nid = 1; nid < n_nodes; ++nid) {
            headss[nid][t] = (volatile size_t*)AllocRELMem(sizeof(size_t));
            *headss[nid][t] = 0;
        }
    } else {
        task_queuess[n][t] = (volatile Op*)AllocNTMem(queue_size * sizeof(Op));
        tailss[n][t] = (volatile size_t*)AllocRELMem(sizeof(size_t));
        memset((void*)task_queuess[n][t], 0, queue_size * sizeof(Op));
        *tailss[n][t] = 0;
    }
        
    pthread_barrier_wait(&global_bar);
#ifndef NO_GEM5
    if (warmup_rounds + n_rounds == 0) {
        if (n == (n_nodes - 1) && t == 0) {
            appl::start_stats();
        }
    }
#endif

    size_t hist_per_node = hist_size / n_nodes;
    size_t next_head = 0;
    size_t next_tail = 0;
    for (int r = 0; r < warmup_rounds + n_rounds; ++r) {

        if (r == warmup_rounds) {
            pthread_barrier_wait(&global_bar);
#ifndef NO_GEM5
            if (n == (n_nodes - 1) && t == 0) {
                appl::start_stats();
            }
#endif
        }

        if (n == 0) {
            for (int nid = 1; nid < n_nodes; ++nid) {
                size_t cur_tail = next_tail;
                next_tail = waitPush(cur_tail, headss[nid][t]);
                insertQueue(cur_tail, task_queuess[nid][t]);
                setTail(next_tail, tailss[nid][t]);
            }
        } else {
            size_t cur_head = next_head;
            next_head = waitPop(cur_head, tailss[n][t]);
            Op op = removeQueue(cur_head, task_queuess[n][t]);
            setHead(next_head, headss[n][t]);

#ifdef SIM_COALEASE
            const int stride = 8;
#else
            const int stride = 1;
#endif            
            int item_id = r * n_nodes * n_threads + n * n_threads + t;
            int item_size = frame_size / threads_per_frame;
            for (int i = 0; i < item_size; ++i) {
#ifdef NO_GEM5
                size_t value = (datas[n][item_id * item_size + i]) % n_bins;
#else
                size_t value = (n+t+i) * 53 % n_bins;
#endif
                if (i % stride == 0)
                    local_hists[n][value] = 1;
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
    while ((opt = getopt(argc, argv, "n:c:t:v:r:w:p:b:f:")) != -1) {
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
                threads_per_frame = std::stoi(optarg);
                break;
            case 'b':
                n_bins = std::stoi(optarg);
                break;
            case 'f':
                if (optarg != nullptr) {
                    input_file = std::string(optarg);
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
    frame_size = m * n;
    assert(((warmup_rounds + n_rounds) * (n_nodes * n_threads)) % threads_per_frame == 0);
    assert(frame_size % threads_per_frame == 0);
    pool_size = (warmup_rounds + n_rounds) * (n_nodes * n_threads) / threads_per_frame;
    hist_size = pool_size * n_bins;

    assert(n_threads <= n_cores);
    assert((hist_size % n_nodes) == 0);

    datas.resize(n_nodes);
    hists.resize(n_nodes);
    local_hists.resize(n_nodes);
    task_queuess.resize(n_nodes);
    headss.resize(n_nodes);
    tailss.resize(n_nodes);
    for (int n = 0; n < n_nodes; ++n) {
        task_queuess[n].resize(n_threads);
        headss[n].resize(n_threads);
        tailss[n].resize(n_threads);
    }

    pthread_barrier_init(&global_bar, nullptr, n_nodes * n_threads);
    pthread_barrier_init(&finish_bar, nullptr, n_nodes * n_cores);

    init_tqh();

    std::vector<std::thread> workers;
    for (int n = 0; n < n_nodes; ++n) {
        for (int t = 0; t < n_threads; ++t) {
            if (n == 0 && t == 0)
                continue;
            workers.emplace_back(run_tqh, n, t);
        }
        for (int t = n_threads; t < n_cores; ++t) {
            workers.emplace_back(no_op);
        }
    }

    run_tqh(0, 0);

    for (auto &worker : workers) {
        worker.join();
    }

    printf("tqh finished\n");

    return 0;
}