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
 * Copyright (c) 1999-2008 Mark D. Hill and David A. Wood
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
 */

#ifndef __MEM_RUBY_STRUCTURES_DOCPUTABLE_HH__
#define __MEM_RUBY_STRUCTURES_DOCPUTABLE_HH__

#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/MachineID.hh"
#include "mem/ruby/common/NetDest.hh"
#include "mem/ruby/common/TypeDefines.hh"

#include <iostream>
#include <set>
#include <unordered_map>
#include <unordered_set>

class DOCPUEntry {
public:
    Cnt_t stCnt = 0;
    std::set<Epoch_t> unCommittedEpochs;
    bool reqNotifySent = false;
    // because slicc does not have for loop, we have to process the st-rel multiple pass,
    // in each pass a req-notify is sent to one pending l2,
    // reqNotifysent marks whether a req-notify for current epoch has been sent
};

class DOCPUTable
{
private:
    std::unordered_map<MachineID, DOCPUEntry, MachineIDHash> m_map;
    Epoch_t curEpoch = 1;

public:
    bool allocated(MachineID);
    void allocate(MachineID);
    void deallocate(MachineID);
    Cnt_t getStCnt(MachineID);
    void incStCnt(MachineID);
    void commitEpoch(MachineID, Epoch_t);
    void addUncommittedEpoch(MachineID, Epoch_t);
    Epoch_t getMaxUncommittedEpoch(MachineID);
    void advanceEpoch();
    Epoch_t getEpoch();
    int getNumPendingL2s(MachineID);
    MachineID popOnePendingL2(MachineID);
    int getNumSentReqNotify();
};

#endif // __MEM_RUBY_STRUCTURES_DOCPUTABLE_HH__