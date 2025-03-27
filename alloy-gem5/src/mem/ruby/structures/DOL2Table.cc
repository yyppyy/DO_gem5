/*
 * SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */


/*
 * Copyright (c) 2012 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Author: Brad Beckmann
 *
 */

#include "mem/ruby/structures/DOL2Table.hh"
#include "base/trace.hh"
#include "debug/DOPROTO.hh"
#include "debug/DOSTORAGE.hh"

bool DOL2Table::allocated(MachineID c_id) {
    return m_map.find(c_id) != m_map.end();
}

void DOL2Table::allocate(MachineID c_id) {
    if (!allocated(c_id)) {
        m_map[c_id] = DOL2Entry{};
    }
}

void DOL2Table::deallocate(MachineID c_id) {
    assert(allocated(c_id));
    m_map.erase(c_id);
}

void DOL2Table::incStCnt(MachineID c_id, Epoch_t epoch) {
    allocated(c_id);
    auto &stCnts = m_map[c_id].stCnts;
    if (stCnts.find(epoch) == stCnts.end()) {
        stCnts[epoch] = 0;
    }
    ++(stCnts[epoch]);

    // for profiling storage overhead
    size_t max_stCnts_size = 0;
    for (auto &i : m_map) {
        if (i.second.stCnts.size() > max_stCnts_size) {
            max_stCnts_size = i.second.stCnts.size();
        }
    }
    DPRINTF(DOSTORAGE, "stCnts %lu\n", max_stCnts_size);
}

Cnt_t DOL2Table::getStCnt(MachineID c_id, Epoch_t epoch) {
    allocated(c_id);
    auto &stCnts = m_map[c_id].stCnts;
    if (stCnts.find(epoch) == stCnts.end()) {
        return 0;
    }
    return stCnts[epoch];
}

void DOL2Table::incNotiCnt(MachineID c_id, Epoch_t epoch) {
    allocated(c_id);
    auto &notiCnts = m_map[c_id].notiCnts;
    if (notiCnts.find(epoch) == notiCnts.end()) {
        notiCnts[epoch] = 0;
    }
    ++(notiCnts[epoch]);

    // for profiling storage overhead
    size_t max_notiCnts_size = 0;
    for (auto &i : m_map) {
        if (i.second.notiCnts.size() > max_notiCnts_size) {
            max_notiCnts_size = i.second.notiCnts.size();
        }
    }
    DPRINTF(DOSTORAGE, "notiCnts %lu\n", max_notiCnts_size);
}

uint32_t DOL2Table::getNotiCnt(MachineID c_id, Epoch_t epoch) {
    allocated(c_id);
    auto &notiCnts = m_map[c_id].notiCnts;
    if (notiCnts.find(epoch) == notiCnts.end()) {
        return 0;
    }
    return notiCnts[epoch];
}

void DOL2Table::markCommittedEpoch(MachineID c_id, Epoch_t epoch) {
    assert(allocated(c_id));
    auto &stCnts = m_map[c_id].stCnts;
    stCnts.erase(epoch);
    auto &notiCnts = m_map[c_id].notiCnts;
    notiCnts.erase(epoch);
    m_map[c_id].maxCommittedEpochs = epoch;
}

bool DOL2Table::canCommit(MachineID c_id, Epoch_t epoch, Cnt_t stCnt,
            uint32_t notiWaitCnt, Epoch_t maxUncommittedEpoch) {
    allocated(c_id);
    auto &stCnts = m_map[c_id].stCnts;
    if (stCnts.find(epoch) == stCnts.end()) {
        if (stCnt != 0) {
            DPRINTF(DOPROTO, "cannot commit epoch[%d] msg_stcnt[%d] exp_stcnt[0]\n", epoch, stCnt);
            return false;
        }
    } else {
        if (stCnts[epoch] != stCnt) {
            DPRINTF(DOPROTO, "cannot commit epoch[%d] msg_stcnt[%d] exp_stcnt[0]\n", epoch, stCnt, stCnts[epoch]);
            return false;
        }
    }
    auto &notiCnts = m_map[c_id].notiCnts;
    if (notiCnts.find(epoch) == notiCnts.end()) {
        if (notiWaitCnt != 0) {
            DPRINTF(DOPROTO, "cannot commit epoch[%d] msg_noticnt[%d] exp_noticnt[0]\n", epoch, notiWaitCnt);
            return false;
        }
    } else {
        if (notiCnts[epoch] != notiWaitCnt) {
            DPRINTF(DOPROTO, "cannot commit epoch[%d] msg_noticnt[%d] exp_noticnt[%d]\n", epoch, notiCnts[epoch]);
            return false;
        }
    }
    if (m_map[c_id].maxCommittedEpochs < maxUncommittedEpoch) {
        DPRINTF(DOPROTO, "cannot commit epoch[%d] msg_maxUncommittedEpoch[%d] exp_maxUncommittedEpoch[%d]\n", epoch, maxUncommittedEpoch, m_map[c_id].maxCommittedEpochs);
        return false;
    }
    DPRINTF(DOPROTO, "can commit epoch[%d]\n", epoch);
    return true;
}

bool DOL2Table::canSendNotify(MachineID c_id, Epoch_t epoch, Cnt_t stCnt,
            Epoch_t maxUncommittedEpoch) {
    allocated(c_id);
    auto &stCnts = m_map[c_id].stCnts;
    if (stCnts.find(epoch) == stCnts.end()) {
        if (stCnt != 0) {
            return false;
        }
    } else {
        if (stCnts[epoch] != stCnt) {
            return false;
        }
    }
    if (m_map[c_id].maxCommittedEpochs < maxUncommittedEpoch) {
        return false;
    }
    return true;
}

void DOL2Table::markNotiSent(MachineID c_id, Epoch_t epoch) {
    allocated(c_id);
    auto &stCnts = m_map[c_id].stCnts;
    stCnts.erase(epoch);
}