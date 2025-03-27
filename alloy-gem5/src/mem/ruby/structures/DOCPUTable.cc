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

#include "mem/ruby/structures/DOCPUTable.hh"
#include "base/trace.hh"
#include "debug/DOPROTO.hh"
#include "debug/DOSTORAGE.hh"

bool DOCPUTable::allocated(MachineID m_id) {
    return m_map.find(m_id) != m_map.end();
}

void DOCPUTable::allocate(MachineID m_id) {
    if (!allocated(m_id)) {
        m_map[m_id] = DOCPUEntry{};
    }
}

void DOCPUTable::deallocate(MachineID m_id) {
    assert(allocated(m_id));
    m_map.erase(m_id);
}

Cnt_t DOCPUTable::getStCnt(MachineID m_id) {
    allocated(m_id);
    return m_map[m_id].stCnt;
}

void DOCPUTable::incStCnt(MachineID m_id) {
    allocated(m_id);
    ++(m_map[m_id].stCnt);
}

void DOCPUTable::commitEpoch(MachineID m_id, Epoch_t epoch) {
    allocated(m_id);
    auto& unCommittedEpochs = m_map[m_id].unCommittedEpochs;
    assert(unCommittedEpochs.find(epoch) != unCommittedEpochs.end());
    unCommittedEpochs.erase(epoch);
}

void DOCPUTable::addUncommittedEpoch(MachineID m_id, Epoch_t epoch) {
    allocated(m_id);
    auto& unCommittedEpochs = m_map[m_id].unCommittedEpochs;
    assert(unCommittedEpochs.find(epoch) == unCommittedEpochs.end());
    unCommittedEpochs.insert(epoch);
    DPRINTF(DOSTORAGE, "unCommittedEpochs %lu\n", unCommittedEpochs.size());
}

Epoch_t DOCPUTable::getMaxUncommittedEpoch(MachineID m_id) {
    allocated(m_id);
    auto& unCommittedEpochs = m_map[m_id].unCommittedEpochs;
    if (unCommittedEpochs.empty()) {
        return 0;
    } else {
        return *(unCommittedEpochs.rbegin());
    }
}

void DOCPUTable::advanceEpoch(void) {
    ++curEpoch;
    for (auto& entry : m_map) {
        entry.second.stCnt = 0;
        entry.second.reqNotifySent = false;
    }
}

Epoch_t DOCPUTable::getEpoch(void) {
    return curEpoch;
}

int DOCPUTable::getNumPendingL2s(MachineID ex_l2_id) {
    uint32_t ret = 0;
    for (const auto& entry : m_map) {
        if ((entry.first != ex_l2_id)
            && ((entry.second.stCnt != 0) || !entry.second.unCommittedEpochs.empty())
            && (!entry.second.reqNotifySent)) {
            DPRINTF(DOPROTO, "count pending L2[%s] = %d\n", entry.first, ret);
            ++ret;
        }
    }
    return ret;
}

MachineID DOCPUTable::popOnePendingL2(MachineID ex_l2_id) {
    for (auto& entry : m_map) {
        if ((entry.first != ex_l2_id)
            && ((entry.second.stCnt != 0) || !entry.second.unCommittedEpochs.empty())
            && (!entry.second.reqNotifySent)) {
            entry.second.reqNotifySent = true;
            DPRINTF(DOPROTO, "pop pending L2[%s]\n", entry.first);
            return entry.first;
        }
    }
    return ex_l2_id;
}

int DOCPUTable::getNumSentReqNotify(void) {
    uint32_t ret = 0;
    for (const auto& entry : m_map) {
        if (entry.second.reqNotifySent) {
            ++ret;
        }
    }
    return ret;
}