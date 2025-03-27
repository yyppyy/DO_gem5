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

#ifndef __MEM_RUBY_SYSTEM_DO_SEQUENCER_HH__
#define __MEM_RUBY_SYSTEM_DO_SEQUENCER_HH__

#include <iostream>
#include <unordered_map>
#include <unordered_set>

#include "mem/protocol/MachineType.hh"
#include "mem/protocol/RubyRequestType.hh"
#include "mem/protocol/SequencerRequestType.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/structures/CacheMemory.hh"
#include "mem/ruby/system/RubyPort.hh"
#include "mem/ruby/system/Sequencer.hh"
#include "params/DOSequencer.hh"

class DOSequencer : public Sequencer
{
public:
    // using Sequencer::Sequencer;
    typedef DOSequencerParams Params;
    DOSequencer(const Params *);
    ~DOSequencer();
    DOSequencer(const DOSequencer& obj) = delete;
    DOSequencer& operator=(const DOSequencer& obj) = delete;
    // void readNTCallback(Addr address, DataBlock& data,
    //                     bool externalHit = false,
    //                     const MachineType mach = MachineType_NUM,
    //                     Cycles initialRequestTime = Cycles(0),
    //                     Cycles forwardRequestTime = Cycles(0),
    //                     Cycles firstResponseTime = Cycles(0));
    void readCallback(Addr address, DataBlock& data,
                        bool externalHit = false,
                        const MachineType mach = MachineType_NUM,
                        Cycles initialRequestTime = Cycles(0),
                        Cycles forwardRequestTime = Cycles(0),
                        Cycles firstResponseTime = Cycles(0));
    void writeCallback(Addr address, DataBlock& data,
                       const bool externalHit = false,
                       const MachineType mach = MachineType_NUM,
                       const Cycles initialRequestTime = Cycles(0),
                       const Cycles forwardRequestTime = Cycles(0),
                       const Cycles firstResponseTime = Cycles(0));
    void DOMarkRemoved();
    // bool DOEmpty() const;
    // void DOPrint(std::ostream& out) const;
protected:
    RequestStatus makeRequest(PacketPtr pkt);
    void issueRequest(PacketPtr pkt, RubyRequestType secondary_type,
                      RubyRequestType primary_type);
    void hitCallback(SequencerRequest* request, DataBlock& data,
                     bool llscSuccess,
                     const MachineType mach, const bool externalHit,
                     const Cycles initialRequestTime,
                     const Cycles forwardRequestTime,
                     const Cycles firstResponseTime);
    RequestStatus DOinsertRequest(PacketPtr pkt, RubyRequestType request_type);

private:
    typedef uint64_t Word_t;
    const Word_t MARKER_ST_NT = 0x01abcdef00000000;
    const Word_t VAL_ST_NT_MASK = 0x00000000ffffffff;
    const Word_t MARKER_ST_NT_SHIFT = 32;
    const Word_t MARKER_ST_REL = 0x01fedcba00000000;
    const Word_t VAL_ST_REL_MASK = 0x00000000ffffffff;
    const Word_t MARKER_ST_REL_SHIFT = 32;

    const Word_t STACK_ADDR = 0x900000;

    // static std::unordered_set<Addr> ST_NT_addrs;
    // static std::unordered_set<Addr> ST_REL_addrs;
    // std::unordered_set<Addr> inFlightLDNTs; // depracated

    DataBlock dummyData;

    typedef std::unordered_multimap<Addr, SequencerRequest*> DORequestTable;
    DORequestTable DO_writeRequestTable;
    DORequestTable DO_readRequestTable;

    bool isSTNT(PacketPtr pkt);
    bool isSTREL(PacketPtr pkt);
    bool isLDNT(PacketPtr pkt);
    bool isLDACQ(PacketPtr pkt);

#ifdef NO_WT
    std::unordered_map<Addr, DataBlock> pktDataCopy;
#endif
};

#endif // __MEM_RUBY_SYSTEM_DO_SEQUENCER_HH__
