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

#pragma once

#include <vector>
#include <cstddef>
#include <cstdint>
#include <vector>
#include <cassert>
#include <cstdio>
#include <iostream>
#include <chrono>

using wordT = uint64_t;
const size_t CACHE_LINE_SIZE = 64;
volatile char* SEND_DUMMY = (volatile char*)(-1);

#define DEBUG

template <typename T>
void Add(volatile T* A, volatile T* B, size_t count) {
    assert(count % sizeof(T) == 0);
    count /= sizeof(T);
    for (size_t i = 0; i < count; ++i) {
        B[i] = A[i] + B[i];
    }
}

inline void Send(volatile char* send_buf,
                 volatile char* recv_buf,
                 volatile wordT* doorbell,
                 size_t count,
                 wordT doorbellVal,
                 int stride_in_word) {
    assert(count % sizeof(wordT) == 0);
    count /= sizeof(wordT);
    volatile wordT* recv_buf_word = (volatile wordT*)recv_buf;

    if (send_buf == SEND_DUMMY) {
        for (size_t i = 0; i < count; i += stride_in_word) {
            recv_buf_word[i] = i;
        }
    } else {
        for (size_t i = 0; i < count; i += stride_in_word) {
            recv_buf_word[i] = ((wordT*)send_buf)[i];
        }
    }
    *doorbell = doorbellVal;
}

inline bool WaitDoorbellNb(volatile wordT* doorbell, wordT doorbellVal) {
    return *doorbell == doorbellVal;
}

inline void WaitDoorbell(volatile wordT* doorbell, wordT doorbellVal) {
    while (*doorbell != doorbellVal);
}

inline void SetDoorbell(volatile wordT* doorbell, wordT doorbellVal) {
    *doorbell = doorbellVal;
}

template <typename T>
bool AllReduceRing(std::vector<volatile char*> &input_bufs, 
                   std::vector<volatile char*[2]> &recv_bufss,
                   std::vector<volatile wordT*[2]> &doorbellss,
                   std::vector<volatile wordT*[2]> &emptyss,
                   size_t count, int nid, int num_nodes,
                   bool apply_op, int stride_in_word) {
    
    assert(count % (sizeof(wordT) * num_nodes) == 0);
    size_t segsize = count / num_nodes;

    int send_to = (nid + 1) % num_nodes;
    int recv_from = (nid + num_nodes - 1) % num_nodes;
    
    int inbi = 0, k = 1;
    size_t block_offset, prevblock;
    volatile char* input_buf = input_bufs[nid];
    volatile char** send_to_bufs = recv_bufss[send_to];
    volatile wordT** send_to_doorbells = doorbellss[send_to];
    volatile wordT** send_to_emptys = emptyss[nid];
    volatile char** recv_bufs = recv_bufss[nid];
    volatile wordT** recv_doorbells = doorbellss[nid];
    volatile wordT** recv_emptys = emptyss[recv_from];

    block_offset = ((size_t)(nid)) * segsize;

    SetDoorbell(recv_emptys[inbi], k);

    WaitDoorbell(send_to_emptys[inbi], k);
    Send(apply_op ? (input_buf + block_offset) : SEND_DUMMY, send_to_bufs[inbi], send_to_doorbells[inbi], segsize, k, stride_in_word);

    for (k = 2; k < num_nodes; ++k) {
        inbi = inbi ^ 0x1;

        SetDoorbell(recv_emptys[inbi], k);

        WaitDoorbell(recv_doorbells[inbi ^ 0x1], k - 1);

        prevblock = (nid + num_nodes - k + 1) % num_nodes;
        block_offset = prevblock * segsize;
        if (apply_op) {
            Add<T>(reinterpret_cast<volatile T*>(recv_bufs[inbi ^ 0x1]),
                        reinterpret_cast<volatile T*>(input_buf + block_offset), segsize);
        }

        WaitDoorbell(send_to_emptys[inbi], k);
        Send(apply_op ? (input_buf + block_offset) : SEND_DUMMY, send_to_bufs[inbi], send_to_doorbells[inbi], segsize, k, stride_in_word);
    }

    WaitDoorbell(recv_doorbells[inbi], k - 1);

    prevblock = (nid + 1) % num_nodes;
    block_offset = prevblock * segsize;
    if (apply_op) {
        Add<T>(reinterpret_cast<volatile T*>(recv_bufs[inbi]),
                    reinterpret_cast<volatile T*>(input_buf + block_offset), segsize);
    }
    
    for (k = 0; k < num_nodes - 1; ++k) {
        inbi = inbi ^ 0x1;

        SetDoorbell(recv_emptys[inbi], k);

        prevblock = (nid + 1 + num_nodes - k) % num_nodes;
        block_offset = prevblock * segsize;

        WaitDoorbell(send_to_emptys[inbi], k);
        Send(apply_op ? (input_buf + block_offset) : SEND_DUMMY,
             apply_op ? (input_bufs[send_to] + block_offset) : send_to_bufs[inbi],
             send_to_doorbells[inbi], segsize, k + num_nodes, stride_in_word);

        WaitDoorbell(recv_doorbells[inbi], k + num_nodes);
    }

    return true;
}

template <typename T>
bool ScatterLinear(std::vector<volatile char*> &input_bufs, 
                   std::vector<volatile char*> &recv_bufs,
                   std::vector<volatile wordT*> &doorbells,
                   std::vector<volatile wordT*> &dones,
                   size_t count,
                   int root, int nid, int num_nodes,
                   bool apply_op, int stride_in_word, int round) {
        
    if (nid == root) {
        for (int i = 0; i < num_nodes; ++i) {
            if (i != root) {
                WaitDoorbell(dones[i], -round);
            }
        }

        for (int i = 0; i < num_nodes; ++i) {
            if (i != root) {
                Send(apply_op ? (input_bufs[i]) : SEND_DUMMY,
                     recv_bufs[i],
                     doorbells[i],
                     count,
                     round,
                     stride_in_word);
            }
        }
        for (int i = 0; i < num_nodes; ++i) {
            if (i != root) {
                WaitDoorbell(doorbells[i], round);
            }
        }
    } else {
        SetDoorbell(dones[nid], -round);
        WaitDoorbell(doorbells[nid], round);
        SetDoorbell(dones[nid], round);
    }
    return true;
}


template <typename T>
bool GatherLinear(std::vector<volatile char*> &input_bufs, 
                   std::vector<volatile char*> &recv_bufs,
                   std::vector<volatile wordT*> &doorbells,
                   std::vector<volatile wordT*> &dones,
                   size_t count,
                   int root, int nid, int num_nodes,
                   bool apply_op, int stride_in_word, int round) {

    if (nid == root) {
        
        for (int i = 0; i < num_nodes; ++i) {
            if (i != root) {
                SetDoorbell(dones[i], -round);
            }
        }

        for (int i = 0; i < num_nodes; ++i) {
            if (i != root) {
                WaitDoorbell(doorbells[i], round);
            }
        }

        for (int i = 0; i < num_nodes; ++i) {
            if (i != root) {
                SetDoorbell(dones[i], round);
            }
        }
    } else {
        WaitDoorbell(dones[nid], -round);
        Send(apply_op ? (input_bufs[nid]) : SEND_DUMMY,
                     recv_bufs[nid],
                     doorbells[nid],
                     count,
                     round,
                     stride_in_word);
        WaitDoorbell(dones[nid], round);
    }
    return true;
}

template <typename T>
bool AllToAllLinear(std::vector<volatile char*> &input_bufs, 
                   std::vector<std::vector<volatile char*>> &recv_bufss,
                   std::vector<std::vector<volatile wordT*>> &doorbellss,
                   std::vector<std::vector<volatile wordT*>> &emptyss,
                   size_t count,
                   int nid, int num_nodes,
                   bool apply_op, int stride_in_word, int round) {

    for (int i = 0; i < num_nodes; ++i) {
        if (i != nid) {
            SetDoorbell(emptyss[nid][i], round);
        }
    }

    size_t done_bitmap = (1 << nid), alldone_bitmap = (1 << num_nodes) - 1;
    assert((size_t)num_nodes <= sizeof(done_bitmap) * 8);
    int i = 0;
    while (done_bitmap != alldone_bitmap) {
        if (!(done_bitmap & (1 << i)) && WaitDoorbellNb(emptyss[i][nid], round)) {
            Send(apply_op ? input_bufs[nid] : SEND_DUMMY,
                    recv_bufss[nid][i],
                    doorbellss[nid][i],
                    count,
                    round,
                    stride_in_word);
            done_bitmap |= (1 << i);
        }
        i = (i + 1) % num_nodes;
    }

    done_bitmap = (1 << nid);
    alldone_bitmap = (1 << num_nodes) - 1;
    i = 0;
    while (done_bitmap != alldone_bitmap) {
        if (!(done_bitmap & (1 << i)) && WaitDoorbellNb(doorbellss[i][nid], round)) {
            done_bitmap |= (1 << i);
        }
        i = (i + 1) % num_nodes;
    }

    return true;
}

template <typename T>
bool ReduceBinaryTree(std::vector<volatile char*> &input_bufs, 
                   std::vector<volatile char*[2]> &recv_bufss,
                   std::vector<volatile wordT*[2]> &doorbellss,
                   std::vector<volatile wordT*> &emptys,
                   size_t count, int root_id, int nid, int num_nodes,
                   bool apply_op, int stride_in_word, int round) {
    assert(root_id == 0);

    int left = (nid << 1) + 1, right = (nid << 1) + 2;
    bool left_exist = left < num_nodes, right_exist = right < num_nodes;
    if (left_exist) {
        SetDoorbell(emptys[left], round);
    }
    if (right_exist) {
        SetDoorbell(emptys[right], round);
    }

    if (left_exist) {
        WaitDoorbell(doorbellss[nid][1], round);
    }
    if (right_exist) {
        WaitDoorbell(doorbellss[nid][0], round);
    }

    if (apply_op) {
        if (left_exist) {
            if (right_exist) {
                Add<T>(reinterpret_cast<volatile T*>(recv_bufss[nid][0]), reinterpret_cast<volatile T*>(recv_bufss[nid][1]), count);
                printf("n[%d] red right [%d]\n", nid, right);
            }
            Add<T>(reinterpret_cast<volatile T*>(recv_bufss[nid][1]), reinterpret_cast<volatile T*>(input_bufs[nid]), count);
            printf("n[%d] red left [%d]\n", nid, left);
        }
    }

    int parent = (nid - 1) >> 1;
    if (nid != root_id) {
        Send(apply_op ? input_bufs[nid] : SEND_DUMMY,
            recv_bufss[parent][nid & 0x1],
            doorbellss[parent][nid & 0x1],
            count,
            round,
            stride_in_word);
    }

    return true;
}