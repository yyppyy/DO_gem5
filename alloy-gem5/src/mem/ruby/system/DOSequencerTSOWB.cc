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

#include "mem/ruby/system/DOSequencer.hh"

#include "arch/x86/ldstflags.hh"
#include "base/logging.hh"
#include "base/str.hh"
#include "cpu/testers/rubytest/RubyTester.hh"
#include "debug/MemoryAccess.hh"
#include "debug/ProtocolTrace.hh"
#include "debug/RubySequencer.hh"
#include "debug/RubyStats.hh"
#include "debug/RRC.hh"
#include "debug/DOACC.hh"
#include "mem/packet.hh"
#include "mem/protocol/PrefetchBit.hh"
#include "mem/protocol/RubyAccessMode.hh"
#include "mem/ruby/profiler/Profiler.hh"
#include "mem/ruby/slicc_interface/RubyRequest.hh"
#include "mem/ruby/system/RubySystem.hh"
#include "sim/system.hh"
#include "sim/process.hh"

DOSequencer *
DOSequencerParams::create()
{
    return new DOSequencer(this);
}

DOSequencer::DOSequencer(const Params *p)
    : Sequencer(p)
{
    // set dummy data
    Word_t dummyVal = MARKER_ST_NT;
    for (size_t offset = 0; offset < RubySystem::getBlockSizeBytes(); offset += sizeof(dummyVal)) {
        dummyData.setData((uint8_t*)&dummyVal, offset, sizeof(dummyVal));
    }
}

DOSequencer::~DOSequencer()
{
}

bool
DOSequencer::isSTNT(PacketPtr pkt)
{
    if (pkt->isWrite() && system->getThreadContext(0)->getProcessPtr()->isSTNTVaddrRange(pkt->getAddr())) {
        DPRINTF(DOACC, "st-nt Addr[0x%x] isAmo[%d] islockedRMW[%d]\n", pkt->getAddr(), pkt->isAtomicOp(), pkt->req->isLockedRMW());
    }
    return false;
    // return pkt->isWrite() && system->getThreadContext(0)->getProcessPtr()->isSTNTVaddrRange(pkt->getAddr());
}

bool
DOSequencer::isSTREL(PacketPtr pkt)
{
    // the reason of this stack addr check is because somehow there are unintended store releases
    // the guess is register spill to stack. So if that happens we must not account the stack write as a release store
    if (pkt->isWrite() && system->getThreadContext(0)->getProcessPtr()->isSTRELVaddrRange(pkt->getAddr())) {
        DPRINTF(DOACC, "st-rel Addr[0x%x] isAmo[%d] islockedRMW[%d]\n", pkt->getAddr(), pkt->isAtomicOp(), pkt->req->isLockedRMW());
    }
    // return pkt->isWrite() && (system->getThreadContext(0)->getProcessPtr()->isSTRELVaddrRange(pkt->getAddr()) ||
            // system->getThreadContext(0)->getProcessPtr()->isSTNTVaddrRange(pkt->getAddr()));
    return false;
}

bool
DOSequencer::isLDNT(PacketPtr pkt)
{       
    if (pkt->isRead() && system->getThreadContext(0)->getProcessPtr()->isSTNTVaddrRange(pkt->getAddr())) {
        DPRINTF(DOACC, "ld-nt Addr[0x%x]\n", pkt->getAddr());
    }
    return false;
    // return pkt->isRead() && system->getThreadContext(0)->getProcessPtr()->isSTNTVaddrRange(pkt->getAddr());
}

bool
DOSequencer::isLDACQ(PacketPtr pkt)
{
    if (pkt->isRead() && system->getThreadContext(0)->getProcessPtr()->isSTRELVaddrRange(pkt->getAddr())) {
        DPRINTF(DOACC, "ld-acq Addr[0x%x]\n", pkt->getAddr());
    }
    // return pkt->isRead() && (system->getThreadContext(0)->getProcessPtr()->isSTRELVaddrRange(pkt->getAddr()) ||
        //    system->getThreadContext(0)->getProcessPtr()->isSTNTVaddrRange(pkt->getAddr()));
    return false;
}

void
DOSequencer::DOMarkRemoved()
{
    m_outstanding_count--;
    assert(m_outstanding_count ==
           DO_writeRequestTable.size() + DO_readRequestTable.size());
}

// bool
// DOSequencer::DOEmpty() const
// {
//     return DO_writeRequestTable.empty() && DO_readRequestTable.empty();
// }

// void
// DOSequencer::DOPrint(ostream& out) const
// {
//     out << "[Sequencer: " << m_version
//         << ", outstanding requests: " << m_outstanding_count
//         << ", read request table: " << DO_readRequestTable
//         << ", write request table: " << DO_writeRequestTable
//         << "]";
// }


RequestStatus
DOSequencer::makeRequest(PacketPtr pkt)
{
    // if (m_outstanding_count >= m_max_outstanding_requests) {
        // return RequestStatus_BufferFull;
    // }
    // for now, assume infinite outstanding requests

    RubyRequestType primary_type = RubyRequestType_NULL;
    RubyRequestType secondary_type = RubyRequestType_NULL;

    if (pkt->isLLSC()) {
        //
        // Alpha LL/SC instructions need to be handled carefully by the cache
        // coherence protocol to ensure they follow the proper semantics. In
        // particular, by identifying the operations as atomic, the protocol
        // should understand that migratory sharing optimizations should not
        // be performed (i.e. a load between the LL and SC should not steal
        // away exclusive permission).
        //
        if (pkt->isWrite()) {
            DPRINTF(RubySequencer, "Issuing SC\n");
            primary_type = RubyRequestType_Store_Conditional;
        } else {
            DPRINTF(RubySequencer, "Issuing LL\n");
            assert(pkt->isRead());
            primary_type = RubyRequestType_Load_Linked;
        }
        secondary_type = RubyRequestType_ATOMIC;
    } else if (pkt->req->isLockedRMW()) {
        //
        // x86 locked instructions are translated to store cache coherence
        // requests because these requests should always be treated as read
        // exclusive operations and should leverage any migratory sharing
        // optimization built into the protocol.
        //
        if (pkt->isWrite()) {
            DPRINTF(RubySequencer, "Issuing Locked RMW Write\n");
            primary_type = RubyRequestType_Locked_RMW_Write;
        } else {
            DPRINTF(RubySequencer, "Issuing Locked RMW Read\n");
            assert(pkt->isRead());
            primary_type = RubyRequestType_Locked_RMW_Read;
        }
        secondary_type = RubyRequestType_ST;
    } else {
        //
        // To support SwapReq, we need to check isWrite() first: a SwapReq
        // should always be treated like a write, but since a SwapReq implies
        // both isWrite() and isRead() are true, check isWrite() first here.
        //
        // moyang: amo are treated as ATOMIC_RETURN, it is handled by L1
        // SLICC controller

        if (pkt->isWrite() && !pkt->isAtomicOp()) {
            //
            // Note: M5 packets do not differentiate ST from RMW_Write
            //
            if (isSTNT(pkt)) {
                primary_type = secondary_type = RubyRequestType_ST_NT;
                // DPRINTF(DOACC, "st-nt Addr[0x%x] isAmo[%d] islockedRMW[%d]\n", pkt->getAddr(), pkt->isAtomicOp(), pkt->req->isLockedRMW());
            } else if (isSTREL(pkt)) {
                primary_type = secondary_type = RubyRequestType_ST_REL;
                // DPRINTF(DOACC, "st-rel Addr[0x%x] isAmo[%d] islockedRMW[%d]\n", pkt->getAddr(), pkt->isAtomicOp(), pkt->req->isLockedRMW());
            } else {
                primary_type = secondary_type = RubyRequestType_ST;
                DPRINTF(RRC, "st-reg Addr[0x%x] isAmo[%d] islockedRMW[%d]\n", pkt->getAddr(), pkt->isAtomicOp(), pkt->req->isLockedRMW());
            }
        } else if (pkt->isRead() && !pkt->isAtomicOp()) {
            if (pkt->req->isInstFetch()) {
                primary_type = secondary_type = RubyRequestType_IFETCH;
                DPRINTF(RRC, "ifetch Addr[0x%x]\n", pkt->getAddr());
            } else {
                bool storeCheck = false;
                // only X86 need the store check
                if (system->getArch() == Arch::X86ISA) {
                    uint32_t flags = pkt->req->getFlags();
                    storeCheck = flags &
                        (X86ISA::StoreCheck << X86ISA::FlagShift);
                }
                if (storeCheck) {
                    primary_type = RubyRequestType_RMW_Read;
                    secondary_type = RubyRequestType_ST;
                } else {
                    
                    if (isLDNT(pkt)) {
                        // DPRINTF(DOACC, "ld-nt Addr[0x%x]\n", pkt->getAddr());
                        primary_type = secondary_type = RubyRequestType_LD_NT;
                    } else if (isLDACQ(pkt)) {
                        // DPRINTF(DOACC, "ld-acq Addr[0x%x]\n", pkt->getAddr());
                        primary_type = secondary_type = RubyRequestType_LD_ACQ;
                    } else {
                        DPRINTF(RRC, "ld-reg Addr[0x%x]\n", pkt->getAddr());
                        primary_type = secondary_type = RubyRequestType_LD;
                    }
                }
            }
        } else if (pkt->cmd == MemCmd::SwapReq && pkt->isAtomicOp()) {
            primary_type = secondary_type = RubyRequestType_ATOMIC_RETURN;
        } else if (pkt->isFlush()) {
          primary_type = secondary_type = RubyRequestType_FLUSH;
        } else {
            panic("Unsupported ruby packet type\n");
        }
    }

    RequestStatus status = DOinsertRequest(pkt, primary_type);
    if (status != RequestStatus_Ready)
        return status;

    issueRequest(pkt, secondary_type, primary_type);

    // TODO: issue hardware prefetches here
    return RequestStatus_Issued;
}

void
DOSequencer::readCallback(Addr address, DataBlock& data,
                        bool externalHit, const MachineType mach,
                        Cycles initialRequestTime,
                        Cycles forwardRequestTime,
                        Cycles firstResponseTime)
{
    assert(address == makeLineAddress(address));
    assert(DO_readRequestTable.count(makeLineAddress(address)));

    RequestTable::iterator i = DO_readRequestTable.find(address);
    // could be a fake LD or LD_NT, doesn't matter which one it is
    assert(i != DO_readRequestTable.end());
    SequencerRequest* request = i->second;

    // DPRINTF(RRC, "ReadCallback [0x%x] m_type[%d]\n", address, request->m_type);

    DO_readRequestTable.erase(i);
    DOMarkRemoved();

    assert((request->m_type == RubyRequestType_LD) ||
           (request->m_type == RubyRequestType_LD_NT) ||
           (request->m_type == RubyRequestType_LD_ACQ) ||
           (request->m_type == RubyRequestType_IFETCH));

    if (request->m_type != RubyRequestType_LD_NT) {
        DPRINTF(RRC, "ReadCallback [0x%x] m_type[%d]\n", address, request->m_type);
        hitCallback(request, data, true, mach, externalHit,
                    initialRequestTime, forwardRequestTime, firstResponseTime);
    } else { // these are "fake" lds triggered by ld-st, should not have effect on the processor
        DPRINTF(RRC, "ReadCallbackFake [0x%x] m_type[%d]\n", address, request->m_type);
        delete request;
    }
}


void
DOSequencer::writeCallback(Addr address, DataBlock& data,
                         const bool externalHit, const MachineType mach,
                         const Cycles initialRequestTime,
                         const Cycles forwardRequestTime,
                         const Cycles firstResponseTime)
{
    assert(address == makeLineAddress(address));
    assert(DO_writeRequestTable.count(makeLineAddress(address)));

    RequestTable::iterator i = DO_writeRequestTable.find(address);
    assert(i != DO_writeRequestTable.end());
    SequencerRequest* request = i->second;

    DO_writeRequestTable.erase(i);
    DOMarkRemoved();

    assert((request->m_type == RubyRequestType_ST) ||
           (request->m_type == RubyRequestType_ST_NT) ||
           (request->m_type == RubyRequestType_ST_REL) ||
           (request->m_type == RubyRequestType_ATOMIC) ||
           (request->m_type == RubyRequestType_ATOMIC_RETURN) ||
           (request->m_type == RubyRequestType_ATOMIC_NO_RETURN) ||
           (request->m_type == RubyRequestType_RMW_Read) ||
           (request->m_type == RubyRequestType_RMW_Write) ||
           (request->m_type == RubyRequestType_Load_Linked) ||
           (request->m_type == RubyRequestType_Store_Conditional) ||
           (request->m_type == RubyRequestType_Locked_RMW_Read) ||
           (request->m_type == RubyRequestType_Locked_RMW_Write) ||
           (request->m_type == RubyRequestType_FLUSH));

    //
    // For Alpha, properly handle LL, SC, and write requests with respect to
    // locked cache blocks.
    //
    // Not valid for Garnet_standalone protocl
    //
    bool success = true;
    if (!m_runningGarnetStandalone)
        success = handleLlsc(address, request);

    // Handle SLICC block_on behavior for Locked_RMW accesses. NOTE: the
    // address variable here is assumed to be a line address, so when
    // blocking buffers, must check line addresses.
    if (request->m_type == RubyRequestType_Locked_RMW_Read) {
        // blockOnQueue blocks all first-level cache controller queues
        // waiting on memory accesses for the specified address that go to
        // the specified queue. In this case, a Locked_RMW_Write must go to
        // the mandatory_q before unblocking the first-level controller.
        // This will block standard loads, stores, ifetches, etc.
        m_controller->blockOnQueue(address, m_mandatory_q_ptr);
    } else if (request->m_type == RubyRequestType_Locked_RMW_Write) {
        m_controller->unblock(address);
    }

    if (request->m_type != RubyRequestType_ST_NT && request->m_type != RubyRequestType_ST_REL) {
        hitCallback(request, data, success, mach, externalHit,
                initialRequestTime, forwardRequestTime, firstResponseTime);
    } else { // fake st triggered by st-nt and st-rel
        DPRINTF(RRC, "WriteCallbackFake [0x%x] m_type[%d]\n", address, request->m_type);
        // if fake st is not wt, the data must be correctly copied to cache here.
#ifdef NO_WT
        DPRINTF(RRC, "WriteCallbackSetDataOnly [0x%x] m_type[%d]\n", address, request->m_type);
        data = pktDataCopy[address];
        pktDataCopy.erase(address);
        DPRINTF(RRC, "set WB data for address[0x%x]\n", address);
        // for now, just simple overwrite, but it should actually first copy the data then set based on offset and size
        // need to be fixed later
#endif
        delete request;
    }
}

void
DOSequencer::hitCallback(SequencerRequest* srequest, DataBlock& data,
                       bool llscSuccess,
                       const MachineType mach, const bool externalHit,
                       const Cycles initialRequestTime,
                       const Cycles forwardRequestTime,
                       const Cycles firstResponseTime)
{
    warn_once("Replacement policy updates recently became the responsibility "
              "of SLICC state machines. Make sure to setMRU() near callbacks "
              "in .sm files!");

    PacketPtr pkt = srequest->pkt;
    Addr request_address(pkt->getAddr());
    RubyRequestType type = srequest->m_type;
    Cycles issued_time = srequest->issue_time;

    assert(curCycle() >= issued_time);
    Cycles total_latency = curCycle() - issued_time;

    // Profile the latency for all demand accesses.
    recordMissLatency(total_latency, type, mach, externalHit, issued_time,
                      initialRequestTime, forwardRequestTime,
                      firstResponseTime, curCycle());
    
    DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s %#x %d cycles\n",
             curTick(), m_version, "Seq",
             llscSuccess ? "Done" : "SC_Failed", "", "",
             printAddress(request_address), total_latency);

    // update the data unless it is a non-data-carrying flush
    if (RubySystem::getWarmupEnabled()) {
        data.setData(pkt->getConstPtr<uint8_t>(),
                     getOffset(request_address), pkt->getSize());
    } else if (!pkt->isFlush()) {
        if ((type == RubyRequestType_LD) ||
            (type == RubyRequestType_LD_NT) ||
            (type == RubyRequestType_LD_ACQ) ||
            (type == RubyRequestType_IFETCH) ||
            (type == RubyRequestType_RMW_Read) ||
            (type == RubyRequestType_Locked_RMW_Read) ||
            (type == RubyRequestType_Load_Linked)) {
            pkt->setData(
                data.getData(getOffset(request_address), pkt->getSize()));
            DPRINTF(RubySequencer, "read: addr 0x%x, data %d, data_blk %s\n",
                        request_address, *(pkt->getConstPtr<uint8_t>()), data);
        } else if (pkt->req->isSwap() || pkt->isAtomicOp()) {
            std::vector<uint8_t> overwrite_val(pkt->getSize());
            if (pkt->isAtomicOp()) {
                memcpy(&overwrite_val[0],
                       data.getData(getOffset(request_address),
                                    pkt->getSize()),
                       pkt->getSize());
                // execute AMO operation on new data
                (*(pkt->getAtomicOp()))(&overwrite_val[0]);
            } else {
                memcpy(&overwrite_val[0], pkt->getConstPtr<uint8_t>(),
                       pkt->getSize());
            }
            // return old value to the packet
            memcpy(pkt->getPtr<uint8_t>(),
                   data.getData(getOffset(request_address), pkt->getSize()),
                   pkt->getSize());
            // update the cache data to overwrite_val
            data.setData(&overwrite_val[0],
                         getOffset(request_address), pkt->getSize());

            DPRINTF(RubySequencer, "swap data %s\n", data);
        } else if (type != RubyRequestType_Store_Conditional || llscSuccess) {
            // Types of stores set the actual data here, apart from
            // failed Store Conditional requests
            
            // should not set data if is a fake st callback.
            // because it will rewrite the dummy data used for ld-nt
            // need to be fixed later

            data.setData(pkt->getConstPtr<uint8_t>(),
                         getOffset(request_address), pkt->getSize());
            DPRINTF(RubySequencer, "write: addr 0x%x, data %d, data_blk %s\n",
                        request_address, *(pkt->getConstPtr<uint8_t>()), data);
        }

        // stats
        if (type == RubyRequestType_LD ||
            type == RubyRequestType_LD_NT ||
            type == RubyRequestType_LD_ACQ ||
            type == RubyRequestType_IFETCH) {
            m_loadCycles += total_latency;
        } else if (type == RubyRequestType_Load_Linked) {
            m_LLCycles += total_latency;
        } else if (type == RubyRequestType_Store_Conditional) {
            m_SCCycles += total_latency;
        } else if (type == RubyRequestType_ATOMIC_RETURN) {
            m_AMOCycles += total_latency;
        } else if (type == RubyRequestType_ST || type == RubyRequestType_ST_NT || type == RubyRequestType_ST_REL) {
            m_storeCycles += total_latency;
        }
    }

    // If using the RubyTester, update the RubyTester sender state's
    // subBlock with the recieved data.  The tester will later access
    // this state.
    if (m_usingRubyTester) {
        DPRINTF(RubySequencer, "hitCallback %s 0x%x using RubyTester\n",
                pkt->cmdString(), pkt->getAddr());
        RubyTester::SenderState* testerSenderState =
            pkt->findNextSenderState<RubyTester::SenderState>();
        assert(testerSenderState);
        testerSenderState->subBlock.mergeFrom(data);
    }

    if (type != RubyRequestType_LD_NT && type != RubyRequestType_ST_NT && type != RubyRequestType_ST_REL) {
        delete srequest;
    }

    // tell processor this is a cache hit or miss, only for logging
    // purposes
    pkt->setCacheHit(!externalHit);

    RubySystem *rs = m_ruby_system;
    if (RubySystem::getWarmupEnabled()) {
        assert(pkt->req);
        delete pkt;
        rs->m_cache_recorder->enqueueNextFetchRequest();
        // DPRINTF(RRC, "pkt deleted warmup\n");
    } else if (RubySystem::getCooldownEnabled()) {
        delete pkt;
        rs->m_cache_recorder->enqueueNextFlushRequest();
        // DPRINTF(RRC, "pkt deleted cooldown\n");
    } else {
        ruby_hit_callback(pkt);
        testDrainComplete();
    }
}

void
DOSequencer::issueRequest(PacketPtr pkt, RubyRequestType secondary_type,
                        RubyRequestType primary_type)
{
    assert(pkt != NULL);
    ContextID proc_id = pkt->req->hasContextId() ?
        pkt->req->contextId() : InvalidContextID;

    ContextID core_id = coreId();

    // If valid, copy the pc to the ruby request
    Addr pc = 0;
    if (pkt->req->hasPC()) {
        pc = pkt->req->getPC();
    }

    std::shared_ptr<RubyRequest> msg;
    if (primary_type == RubyRequestType_ST_NT || primary_type == RubyRequestType_ST_REL) {
        // set write mask for st-nt and st-rel (for all accesses currently)
        uint32_t blockSize = RubySystem::getBlockSizeBytes();
        uint32_t offset = pkt->getAddr() - makeLineAddress(pkt->getAddr());
        std::vector<bool> accessMask(blockSize, false);
        DataBlock dataBlock;
        dataBlock.clear();
        for (int j = 0; j < pkt->getSize(); j++) {
            accessMask[offset + j] = true;
        }
        dataBlock.setData(pkt->getPtr<uint8_t>(), offset, pkt->getSize()); 

        // check if the packet has data as for example prefetch and flush
        // requests do not
        msg = std::make_shared<RubyRequest>(clockEdge(), pkt->getAddr(),
                                        pkt->isFlush() ?
                                        nullptr : pkt->getPtr<uint8_t>(),
                                        pkt->getSize(), pc, secondary_type,
                                        primary_type, RubyAccessMode_Supervisor,
                                        pkt,
                                        accessMask, dataBlock,
                                        PrefetchBit_No, proc_id, core_id);

        DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s %#x %s\n",
                curTick(), m_version, "Seq", "Begin", "", "",
                printAddress(msg->getPhysicalAddress()),
                RubyRequestType_to_string(secondary_type));
    } else {
        msg = std::make_shared<RubyRequest>(clockEdge(), pkt->getAddr(),
                                        pkt->isFlush() ?
                                        nullptr : pkt->getPtr<uint8_t>(),
                                        pkt->getSize(), pc, secondary_type,
                                        primary_type, RubyAccessMode_Supervisor,
                                        pkt, PrefetchBit_No, proc_id, core_id);

        DPRINTFR(ProtocolTrace, "%15s %3s %10s%20s %6s>%-6s %#x %s\n",
                curTick(), m_version, "Seq", "Begin", "", "",
                printAddress(msg->getPhysicalAddress()),
                RubyRequestType_to_string(secondary_type));
    }

#ifdef NO_WT
    Addr address(makeLineAddress(pkt->getAddr()));
    uint32_t offset = pkt->getAddr() - makeLineAddress(pkt->getAddr());
    pktDataCopy[address] = DataBlock{};
    pktDataCopy[address].clear();
    pktDataCopy[address].setData(pkt->getPtr<uint8_t>(), offset, pkt->getSize());
    DPRINTF(RRC, "copy WB data for address[0x%x] offset[%d]\n", address, offset);
#endif

    // The Sequencer currently assesses instruction and data cache hit latency
    // for the top-level caches at the beginning of a memory access.
    // TODO: Eventually, this latency should be moved to represent the actual
    // cache access latency portion of the memory access. This will require
    // changing cache controller protocol files to assess the latency on the
    // access response path.
    Cycles latency(0);  // Initialize to zero to catch misconfigured latency
    if (secondary_type == RubyRequestType_IFETCH)
        latency = m_inst_cache_hit_latency;
    else
        latency = m_data_cache_hit_latency;

    // Send the message to the cache controller
    assert(latency > 0);

    assert(m_mandatory_q_ptr != NULL);
    // print something here...why double enqueue?
    // DPRINTF(DOACC, "mandatory enqueue ptype[%s] stype[%s]\n", RubyRequestType_to_string(primary_type), RubyRequestType_to_string(secondary_type));
    m_mandatory_q_ptr->enqueue(msg, clockEdge(), cyclesToTicks(latency));

    // stats
    if (primary_type == RubyRequestType_Load_Linked) {
        m_numLL++;
    } else if (primary_type == RubyRequestType_Store_Conditional) {
        m_numSC++;
    } else if (primary_type == RubyRequestType_LD ||
               primary_type == RubyRequestType_LD_NT ||
               primary_type == RubyRequestType_IFETCH) {
        m_numLoad++;
    } else if (primary_type == RubyRequestType_ST || primary_type == RubyRequestType_ST_NT || primary_type == RubyRequestType_ST_REL) {
        m_numStore++;
    } else if (primary_type == RubyRequestType_ATOMIC_RETURN) {
        m_numAMO++;
    }
}

RequestStatus
DOSequencer::DOinsertRequest(PacketPtr pkt, RubyRequestType request_type)
{
    assert(m_outstanding_count ==
        (DO_writeRequestTable.size() + DO_readRequestTable.size()));

    // See if we should schedule a deadlock check
    if (!deadlockCheckEvent.scheduled() &&
        drainState() != DrainState::Draining) {
        schedule(deadlockCheckEvent, clockEdge(m_deadlock_threshold));
    }

    Addr line_addr = makeLineAddress(pkt->getAddr());

    // Check if the line is blocked for a Locked_RMW
    if (m_controller->isBlocked(line_addr) &&
        (request_type != RubyRequestType_Locked_RMW_Write)) {
        // Return that this request's cache line address aliases with
        // a prior request that locked the cache line. The request cannot
        // proceed until the cache line is unlocked by a Locked_RMW_Write
        return RequestStatus_Aliased;
    }

    // Create a default entry, mapping the address to NULL, the cast is
    // there to make gcc 4.4 happy
    RequestTable::value_type default_entry(line_addr,
                                           (SequencerRequest*) NULL);

    if ((request_type == RubyRequestType_ST) ||
        (request_type == RubyRequestType_ST_NT) ||
        (request_type == RubyRequestType_ST_REL) ||
        (request_type == RubyRequestType_RMW_Read) ||
        (request_type == RubyRequestType_RMW_Write) ||
        (request_type == RubyRequestType_Load_Linked) ||
        (request_type == RubyRequestType_Store_Conditional) ||
        (request_type == RubyRequestType_Locked_RMW_Read) ||
        (request_type == RubyRequestType_Locked_RMW_Write) ||
        (request_type == RubyRequestType_FLUSH) ||
        (request_type == RubyRequestType_ATOMIC) ||
        (request_type == RubyRequestType_ATOMIC_RETURN) ||
        (request_type == RubyRequestType_ATOMIC_NO_RETURN)) {

        // for directory ordering
        // if (request_type == RubyRequestType_ST_REL && m_outstanding_count > 0) {
        //     // retry later, or should return RequestStatus_Bufferfull?
        //     return RequestStatus_Aliased;
        // }

        // Check if there is any outstanding read request for the same
        // cache line.
        // if (DO_readRequestTable.count(line_addr) > 0) {
        //     m_store_waiting_on_load++;
        //     return RequestStatus_Aliased;
        // }

        // std::pair<RequestTable::iterator, bool> r =
        auto r =
            DO_writeRequestTable.insert(default_entry);
        // if (r.second) {
            assert(r != DO_writeRequestTable.end());
            r->second = new SequencerRequest(pkt, request_type, curCycle());
            if (request_type == RubyRequestType_ST_NT || request_type == RubyRequestType_ST_REL) {
                // TimingSimpleCPU does not care about st's return value, so return dummy data is fine.
                hitCallback(r->second, dummyData, true, MachineType_NUM, true,
                    Cycles(0), Cycles(0), Cycles(0));
            }
            m_outstanding_count++;
            
        // } else {
        //   // There is an outstanding write request for the cache line
        //   m_store_waiting_on_store++;
        //   return RequestStatus_Aliased;
        // }
    } else {
        // Check if there is any outstanding write request for the same
        // cache line.
        // if (DO_writeRequestTable.count(line_addr) > 0) {
        //     m_load_waiting_on_store++;
        //     return RequestStatus_Aliased;
        // }

        // std::pair<RequestTable::iterator, bool> r =
        auto r = 
            DO_readRequestTable.insert(default_entry);

        // if (r.second) {
            assert(r != DO_readRequestTable.end());
            r->second = new SequencerRequest(pkt, request_type, curCycle());
            if (request_type == RubyRequestType_LD_NT) {
                hitCallback(r->second, dummyData, true, MachineType_NUM, true,
                    Cycles(0), Cycles(0), Cycles(0));
            }
            m_outstanding_count++;
        // } else {
        //     // There is an outstanding read request for the cache line
        //     // DPRINTF(RRC, "aliased, line_addr[0x%x]\n", line_addr);
        //     m_load_waiting_on_load++;
        //     return RequestStatus_Aliased;
        // }
    }

    m_outstandReqHist.sample(m_outstanding_count);
    assert(m_outstanding_count ==
        (DO_writeRequestTable.size() + DO_readRequestTable.size()));

    return RequestStatus_Ready;
}
