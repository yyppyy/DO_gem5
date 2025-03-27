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
 * Copyright 2014 Google, Inc.
 * Copyright (c) 2010-2013,2015,2017 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2002-2005 The Regents of The University of Michigan
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
 * Authors: Steve Reinhardt
 */

#include "arch/locked_mem.hh"
#include "arch/mmapped_ipr.hh"
#include "arch/registers.hh"
#include "arch/utility.hh"
#include "config/the_isa.hh"
#include "cpu/exetrace.hh"
#include "cpu/simple/timing.hh"
#include "debug/Config.hh"
#include "debug/Drain.hh"
#include "debug/ExecFaulting.hh"
#include "debug/Mwait.hh"
#include "debug/SimpleCPU.hh"
#include "debug/ULI.hh"
#include "mem/packet.hh"
#include "mem/packet_access.hh"
#include "params/TimingSimpleCPU.hh"
#include "sim/faults.hh"
#include "sim/full_system.hh"
#include "sim/system.hh"

using namespace std;
using namespace TheISA;

void
TimingSimpleCPU::init()
{
    BaseSimpleCPU::init();
}

void
TimingSimpleCPU::TimingCPUPort::TickEvent::schedule(PacketPtr _pkt, Tick t)
{
    pkt = _pkt;
    cpu->schedule(this, t);
}

TimingSimpleCPU::TimingSimpleCPU(TimingSimpleCPUParams *p)
    : BaseSimpleCPU(p), fetchTranslation(this), icachePort(this),
      dcachePort(this),
      outReqPort(this, ".out_nw_req_port"),
      inReqPort(this, ".in_nw_req_port"),
      ifetch_pkt(NULL), dcache_pkt(NULL), previousCycle(0),
      previousInsts(0),fetchEvent([this]{ fetch(); }, name())
{
    _status = Idle;
#if THE_ISA == RISCV_ISA
    icache_start_cycle = Cycles(0);
    dcache_start_cycle = Cycles(0);

    in_runtime = false;

    active_message_start_cycle = Cycles(0);

    num_active_message_ack      = 0;
    active_message_ack_latency  = 0;
    num_active_message_nack     = 0;
    active_message_nack_latency = 0;
#endif
}



TimingSimpleCPU::~TimingSimpleCPU()
{
}

DrainState
TimingSimpleCPU::drain()
{
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    if (switchedOut())
        return DrainState::Drained;

    if (_status == Idle ||
        (_status == BaseSimpleCPU::Running && isDrained())) {
        DPRINTF(Drain, "No need to drain.\n");
        activeThreads.clear();
        return DrainState::Drained;
    } else {
        DPRINTF(Drain, "Requesting drain.\n");

        // The fetch event can become descheduled if a drain didn't
        // succeed on the first attempt. We need to reschedule it if
        // the CPU is waiting for a microcode routine to complete.
        if (_status == BaseSimpleCPU::Running && !fetchEvent.scheduled())
            schedule(fetchEvent, clockEdge());

        return DrainState::Draining;
    }
}

void
TimingSimpleCPU::drainResume()
{
    assert(!fetchEvent.scheduled());
    if (switchedOut())
        return;

    DPRINTF(SimpleCPU, "Resume\n");
    verifyMemoryMode();

    assert(!threadContexts.empty());

    _status = BaseSimpleCPU::Idle;

    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (threadInfo[tid]->thread->status() == ThreadContext::Active) {
            threadInfo[tid]->notIdleFraction = 1;

            activeThreads.push_back(tid);

            _status = BaseSimpleCPU::Running;

            // Fetch if any threads active
            if (!fetchEvent.scheduled()) {
                schedule(fetchEvent, nextCycle());
            }
        } else {
            threadInfo[tid]->notIdleFraction = 0;
        }
    }

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();

    system->totalNumInsts = 0;
}

bool
TimingSimpleCPU::tryCompleteDrain()
{
    if (drainState() != DrainState::Draining)
        return false;

    DPRINTF(Drain, "tryCompleteDrain.\n");
    if (!isDrained())
        return false;

    DPRINTF(Drain, "CPU done draining, processing drain event\n");
    signalDrainDone();

    return true;
}

void
TimingSimpleCPU::switchOut()
{
    SimpleExecContext& t_info = *threadInfo[curThread];
    M5_VAR_USED SimpleThread* thread = t_info.thread;

    BaseSimpleCPU::switchOut();

    assert(!fetchEvent.scheduled());
    assert(_status == BaseSimpleCPU::Running || _status == Idle);
    assert(!t_info.stayAtPC);
    assert(thread->microPC() == 0);

    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);
}


void
TimingSimpleCPU::takeOverFrom(BaseCPU *oldCPU)
{
    BaseSimpleCPU::takeOverFrom(oldCPU);

    previousCycle = curCycle();
    previousInsts = totalInsts();
}

void
TimingSimpleCPU::verifyMemoryMode() const
{
    if (!system->isTimingMode()) {
        fatal("The timing CPU requires the memory system to be in "
              "'timing' mode.\n");
    }
}

void
TimingSimpleCPU::activateContext(ThreadID thread_num)
{
    DPRINTF(SimpleCPU, "ActivateContext %d\n", thread_num);

    assert(thread_num < numThreads);

    threadInfo[thread_num]->notIdleFraction = 1;
    if (_status == BaseSimpleCPU::Idle)
        _status = BaseSimpleCPU::Running;

    // kick things off by initiating the fetch of the next instruction
    if (!fetchEvent.scheduled())
        schedule(fetchEvent, clockEdge(Cycles(0)));

    if (std::find(activeThreads.begin(), activeThreads.end(), thread_num)
         == activeThreads.end()) {
        activeThreads.push_back(thread_num);
    }

    BaseCPU::activateContext(thread_num);
}


void
TimingSimpleCPU::suspendContext(ThreadID thread_num)
{
    DPRINTF(SimpleCPU, "SuspendContext %d\n", thread_num);

    assert(thread_num < numThreads);
    activeThreads.remove(thread_num);

    if (_status == Idle)
        return;

    assert(_status == BaseSimpleCPU::Running);

    threadInfo[thread_num]->notIdleFraction = 0;

    if (activeThreads.empty()) {
        _status = Idle;

        if (fetchEvent.scheduled()) {
            deschedule(fetchEvent);
        }
    }

    BaseCPU::suspendContext(thread_num);
}

bool
TimingSimpleCPU::handleReadPacket(PacketPtr pkt)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    const RequestPtr &req = pkt->req;

    // We're about the issues a locked load, so tell the monitor
    // to start caring about this address
    if (pkt->isRead() && pkt->req->isLLSC()) {
        TheISA::handleLockedRead(thread, pkt->req);
    }
    if (req->isMmappedIpr()) {
        Cycles delay = TheISA::handleIprRead(thread->getTC(), pkt);
        new IprEvent(pkt, this, clockEdge(delay));
        _status = DcacheWaitResponse;
        dcache_pkt = NULL;
    } else if (!dcachePort.sendTimingReq(pkt)) {
        _status = DcacheRetry;
        dcache_pkt = pkt;
    } else {
        _status = DcacheWaitResponse;
        // memory system takes ownership of packet
        dcache_pkt = NULL;
    }
    return dcache_pkt == NULL;
}

void
TimingSimpleCPU::sendData(const RequestPtr &req, uint8_t *data, uint64_t *res,
                          bool read)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    PacketPtr pkt = buildPacket(req, read);
    pkt->dataDynamic<uint8_t>(data);

#if THE_ISA == RISCV_ISA
    // We add the CSR_VALUE17 to the packet and send to the memory system
    ThreadContext* tc = getContext(0);
    uint64_t val17 = tc->readMiscReg(TheISA::MISCREG_VALUE17);
    pkt->cache_level(val17);
#endif

    if (req->getFlags().isSet(Request::NO_ACCESS)) {
        assert(!dcache_pkt);
        pkt->makeResponse();
        completeDataAccess(pkt);
    } else if (read) {
        handleReadPacket(pkt);
    } else {
        bool do_access = true;  // flag to suppress cache access

        if (req->isLLSC()) {
            do_access = TheISA::handleLockedWrite(thread, req, dcachePort.cacheBlockMask);
        } else if (req->isCondSwap()) {
            assert(res);
            req->setExtraData(*res);
        }

        if (do_access) {
            dcache_pkt = pkt;
            handleWritePacket();
            threadSnoop(pkt, curThread);
        } else {
            _status = DcacheWaitResponse;
            completeDataAccess(pkt);
        }
    }
}

void
TimingSimpleCPU::sendSplitData(const RequestPtr &req1, const RequestPtr &req2,
                               const RequestPtr &req, uint8_t *data, bool read)
{
    PacketPtr pkt1, pkt2;
    buildSplitPacket(pkt1, pkt2, req1, req2, req, data, read);
#if THE_ISA == RISCV_ISA
    // We add the CSR_VALUE17 to the packet and send to the memory system
    ThreadContext* tc = getContext(0);
    uint64_t val17 = tc->readMiscReg(TheISA::MISCREG_VALUE17);
    pkt1->cache_level(val17);
    pkt2->cache_level(val17);
#endif
    if (req->getFlags().isSet(Request::NO_ACCESS)) {
        assert(!dcache_pkt);
        pkt1->makeResponse();
        completeDataAccess(pkt1);
    } else if (read) {
        SplitFragmentSenderState * send_state =
            dynamic_cast<SplitFragmentSenderState *>(pkt1->senderState);
        if (handleReadPacket(pkt1)) {
            send_state->clearFromParent();
            send_state = dynamic_cast<SplitFragmentSenderState *>(
                    pkt2->senderState);
            if (handleReadPacket(pkt2)) {
                send_state->clearFromParent();
            }
        }
    } else {
        dcache_pkt = pkt1;
        SplitFragmentSenderState * send_state =
            dynamic_cast<SplitFragmentSenderState *>(pkt1->senderState);
        if (handleWritePacket()) {
            send_state->clearFromParent();
            dcache_pkt = pkt2;
            send_state = dynamic_cast<SplitFragmentSenderState *>(
                    pkt2->senderState);
            if (handleWritePacket()) {
                send_state->clearFromParent();
            }
        }
    }
}

void
TimingSimpleCPU::translationFault(const Fault &fault)
{
    // fault may be NoFault in cases where a fault is suppressed,
    // for instance prefetches.
    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);

    if (traceData) {
        // Since there was a fault, we shouldn't trace this instruction.
        delete traceData;
        traceData = NULL;
    }

    postExecute();

    advanceInst(fault);
}

PacketPtr
TimingSimpleCPU::buildPacket(const RequestPtr &req, bool read)
{
    return read ? Packet::createRead(req) : Packet::createWrite(req);
}

void
TimingSimpleCPU::buildSplitPacket(PacketPtr &pkt1, PacketPtr &pkt2,
        const RequestPtr &req1, const RequestPtr &req2, const RequestPtr &req,
        uint8_t *data, bool read)
{
    pkt1 = pkt2 = NULL;

    assert(!req1->isMmappedIpr() && !req2->isMmappedIpr());

    if (req->getFlags().isSet(Request::NO_ACCESS)) {
        pkt1 = buildPacket(req, read);
        return;
    }

    pkt1 = buildPacket(req1, read);
    pkt2 = buildPacket(req2, read);

    PacketPtr pkt = new Packet(req, pkt1->cmd.responseCommand());

    pkt->dataDynamic<uint8_t>(data);
    pkt1->dataStatic<uint8_t>(data);
    pkt2->dataStatic<uint8_t>(data + req1->getSize());

    SplitMainSenderState * main_send_state = new SplitMainSenderState;
    pkt->senderState = main_send_state;
    main_send_state->fragments[0] = pkt1;
    main_send_state->fragments[1] = pkt2;
    main_send_state->outstanding = 2;
    pkt1->senderState = new SplitFragmentSenderState(pkt, 0);
    pkt2->senderState = new SplitFragmentSenderState(pkt, 1);
}

Fault
TimingSimpleCPU::initiateMemRead(Addr addr, unsigned size,
                                 Request::Flags flags)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    Fault fault;
    const int asid = 0;
    const Addr pc = thread->instAddr();
    unsigned block_size = cacheLineSize();
    BaseTLB::Mode mode = BaseTLB::Read;

    if (traceData)
        traceData->setMem(addr, size, flags);

    RequestPtr req = std::make_shared<Request>(
        asid, addr, size, flags, dataMasterId(), pc,
        thread->contextId());

    req->taskId(taskId());

    Addr split_addr = roundDown(addr + size - 1, block_size);
    assert(split_addr <= addr || split_addr - addr < block_size);

    _status = DTBWaitResponse;
    if (split_addr > addr) {
        RequestPtr req1, req2;
        assert(!req->isLLSC() && !req->isSwap());
        req->splitOnVaddr(split_addr, req1, req2);

        WholeTranslationState *state =
            new WholeTranslationState(req, req1, req2, new uint8_t[size],
                                      NULL, mode);
        DataTranslation<TimingSimpleCPU *> *trans1 =
            new DataTranslation<TimingSimpleCPU *>(this, state, 0);
        DataTranslation<TimingSimpleCPU *> *trans2 =
            new DataTranslation<TimingSimpleCPU *>(this, state, 1);

        thread->dtb->translateTiming(req1, thread->getTC(), trans1, mode);
        thread->dtb->translateTiming(req2, thread->getTC(), trans2, mode);
    } else {
        WholeTranslationState *state =
            new WholeTranslationState(req, new uint8_t[size], NULL, mode);
        DataTranslation<TimingSimpleCPU *> *translation
            = new DataTranslation<TimingSimpleCPU *>(this, state);
        thread->dtb->translateTiming(req, thread->getTC(), translation, mode);
    }

    return NoFault;
}

void
TimingSimpleCPU::handleMemFence()
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    Fault fault;
    const Addr pc = thread->instAddr();

    // make a memfence request that has address of 0
    RequestPtr req = std::make_shared<Request>(0,   // paddr
                                               0,   // size
                                               0,   // flag
                                               dataMasterId(),
                                               pc,
                                               thread->contextId());
    req->taskId(taskId());
    req->setPaddr(0);

    if (curStaticInst->isReadBarrier()) {
        req->setFlags(Request::ACQUIRE);
        appl_num_self_invalidation += 1;
    }

    if (curStaticInst->isWriteBarrier()) {
        req->setFlags(Request::RELEASE);
        appl_num_self_writeback += 1;
    }

    // If neither read or write, we assume it's both a read and write
    // barrier.
    if (!curStaticInst->isReadBarrier() &&
        !curStaticInst->isWriteBarrier()) {
        req->setFlags(Request::ACQUIRE);
        req->setFlags(Request::RELEASE);
        appl_num_self_invalidation += 1;
        appl_num_self_writeback    += 1;
    }

    // create a new memfence packet
    PacketPtr pkt = new Packet(req, MemCmd::MemFenceReq);

    ThreadContext* tc = getContext(0);
    pkt->inv_wb_type(tc->readMiscReg(TheISA::MISCREG_VALUE15));
    pkt->inv_wb_level(tc->readMiscReg(TheISA::MISCREG_VALUE16));

    if (!dcachePort.sendTimingReq(pkt)) {
        _status = DcacheRetry;
        dcache_pkt = pkt;
    } else {
        _status = DcacheWaitResponse;
        // memory system takes ownership of packet
        dcache_pkt = NULL;
    }
}

bool
TimingSimpleCPU::handleWritePacket()
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    const RequestPtr &req = dcache_pkt->req;
    if (req->isMmappedIpr()) {
        Cycles delay = TheISA::handleIprWrite(thread->getTC(), dcache_pkt);
        new IprEvent(dcache_pkt, this, clockEdge(delay));
        _status = DcacheWaitResponse;
        dcache_pkt = NULL;
    } else if (!dcachePort.sendTimingReq(dcache_pkt)) {
        _status = DcacheRetry;
    } else {
        _status = DcacheWaitResponse;
        // memory system takes ownership of packet
        dcache_pkt = NULL;
    }
    return dcache_pkt == NULL;
}

Fault
TimingSimpleCPU::writeMem(uint8_t *data, unsigned size,
                          Addr addr, Request::Flags flags, uint64_t *res)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    uint8_t *newData = new uint8_t[size];
    const int asid = 0;
    const Addr pc = thread->instAddr();
    unsigned block_size = cacheLineSize();
    BaseTLB::Mode mode = BaseTLB::Write;

    if (data == NULL) {
        assert(flags & Request::STORE_NO_DATA);
        // This must be a cache block cleaning request
        memset(newData, 0, size);
    } else {
        memcpy(newData, data, size);
    }

    if (traceData)
        traceData->setMem(addr, size, flags);

    RequestPtr req = std::make_shared<Request>(
        asid, addr, size, flags, dataMasterId(), pc,
        thread->contextId());

    req->taskId(taskId());

    Addr split_addr = roundDown(addr + size - 1, block_size);
    assert(split_addr <= addr || split_addr - addr < block_size);

    _status = DTBWaitResponse;
    if (split_addr > addr) {
        RequestPtr req1, req2;
        assert(!req->isLLSC() && !req->isSwap());
        req->splitOnVaddr(split_addr, req1, req2);

        WholeTranslationState *state =
            new WholeTranslationState(req, req1, req2, newData, res, mode);
        DataTranslation<TimingSimpleCPU *> *trans1 =
            new DataTranslation<TimingSimpleCPU *>(this, state, 0);
        DataTranslation<TimingSimpleCPU *> *trans2 =
            new DataTranslation<TimingSimpleCPU *>(this, state, 1);

        thread->dtb->translateTiming(req1, thread->getTC(), trans1, mode);
        thread->dtb->translateTiming(req2, thread->getTC(), trans2, mode);
    } else {
        WholeTranslationState *state =
            new WholeTranslationState(req, newData, res, mode);
        DataTranslation<TimingSimpleCPU *> *translation =
            new DataTranslation<TimingSimpleCPU *>(this, state);
        thread->dtb->translateTiming(req, thread->getTC(), translation, mode);
    }

    // Translation faults will be returned via finishTranslation()
    return NoFault;
}

Fault
TimingSimpleCPU::initiateMemAMO(Addr addr, unsigned size,
                                Request::Flags flags,
                                AtomicOpFunctor *amo_op)
{
    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    Fault fault;
    const int asid = 0;
    const Addr pc = thread->instAddr();
    unsigned block_size = cacheLineSize();
    BaseTLB::Mode mode = BaseTLB::Write;

    if (traceData)
        traceData->setMem(addr, size, flags);

#if THE_ISA == RISCV_ISA
    // moyang TC3: this a hacky way to implement cache write-back and
    // invalidation "instructions":
    //
    // amo[xx].w.aq zero, zero 0(zero) -> cache invalidation
    // amo[xx].w.rl zero, zero 0(zero) -> cache write-back
    //
    // ignore this request if address is zero, without generating a
    // pagetable faults
    if (addr == 0) {
        return NoFault;
    }
#endif

    RequestPtr req = make_shared<Request>(asid, addr, size, flags,
                            dataMasterId(), pc, thread->contextId(), amo_op);

    assert(req->hasAtomicOpFunctor());

    req->taskId(taskId());

    Addr split_addr = roundDown(addr + size - 1, block_size);

    // AMO requests that access across a cache line boundary are not
    // allowed since the cache does not guarantee AMO ops to be executed
    // atomically in two cache lines
    // For ISAs such as x86 that requires AMO operations to work on
    // accesses that cross cache-line boundaries, the cache needs to be
    // modified to support locking both cache lines to guarantee the
    // atomicity.
    if (split_addr > addr) {
        panic("AMO requests should not access across a cache line boundary\n");
    }

    _status = DTBWaitResponse;

    WholeTranslationState *state =
        new WholeTranslationState(req, new uint8_t[size], NULL, mode);
    DataTranslation<TimingSimpleCPU *> *translation
        = new DataTranslation<TimingSimpleCPU *>(this, state);
    thread->dtb->translateTiming(req, thread->getTC(), translation, mode);

    return NoFault;
}

void
TimingSimpleCPU::threadSnoop(PacketPtr pkt, ThreadID sender)
{
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (tid != sender) {
            if (getCpuAddrMonitor(tid)->doMonitor(pkt)) {
                wakeup(tid);
            }
            TheISA::handleLockedSnoop(threadInfo[tid]->thread, pkt,
                    dcachePort.cacheBlockMask);
        }
    }
}

void
TimingSimpleCPU::finishTranslation(WholeTranslationState *state)
{
    _status = BaseSimpleCPU::Running;

    if (state->getFault() != NoFault) {
        if (state->isPrefetch()) {
            state->setNoFault();
        }
        delete [] state->data;
        state->deleteReqs();
        translationFault(state->getFault());
    } else {
        if (!state->isSplit) {
            sendData(state->mainReq, state->data, state->res,
                     state->mode == BaseTLB::Read);
        } else {
            sendSplitData(state->sreqLow, state->sreqHigh, state->mainReq,
                          state->data, state->mode == BaseTLB::Read);
        }
    }

    delete state;
}


void
TimingSimpleCPU::fetch()
{
    // Change thread if multi-threaded
    swapActiveThread();

    SimpleExecContext &t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    DPRINTF(SimpleCPU, "Fetch\n");

    if (!curStaticInst || !curStaticInst->isDelayedCommit()) {
        checkForInterrupts();
        checkPcEventQueue();
    }

    // We must have just got suspended by a PC event
    if (_status == Idle)
        return;

    TheISA::PCState pcState = thread->pcState();
    bool needToFetch = !isRomMicroPC(pcState.microPC()) &&
                       !curMacroStaticInst;

    if (needToFetch) {
        _status = BaseSimpleCPU::Running;
        RequestPtr ifetch_req = std::make_shared<Request>();
        ifetch_req->taskId(taskId());
        ifetch_req->setContext(thread->contextId());
        setupFetchRequest(ifetch_req);
        DPRINTF(SimpleCPU, "Translating address %#x\n", ifetch_req->getVaddr());
        thread->itb->translateTiming(ifetch_req, thread->getTC(),
                &fetchTranslation, BaseTLB::Execute);
    } else {
        _status = IcacheWaitResponse;
        completeIfetch(NULL);

        updateCycleCounts();
        updateCycleCounters(BaseCPU::CPU_STATE_ON);
    }
}


void
TimingSimpleCPU::sendFetch(const Fault &fault, const RequestPtr &req,
                           ThreadContext *tc)
{
    if (fault == NoFault) {
        DPRINTF(SimpleCPU, "Sending fetch for addr %#x(pa: %#x)\n",
                req->getVaddr(), req->getPaddr());
        ifetch_pkt = new Packet(req, MemCmd::ReadReq);
        ifetch_pkt->dataStatic(&inst);
        ifetch_pkt->cache_level(0);
        DPRINTF(SimpleCPU, " -- pkt addr: %#x\n", ifetch_pkt->getAddr());

        if (!icachePort.sendTimingReq(ifetch_pkt)) {
            // Need to wait for retry
            _status = IcacheRetry;
        } else {
            // Need to wait for cache to respond
            _status = IcacheWaitResponse;
            // ownership of packet transferred to memory system
            ifetch_pkt = NULL;

            icache_start_cycle = curCycle();
        }
    } else {
        DPRINTF(SimpleCPU, "Translation of addr %#x faulted\n", req->getVaddr());
        // fetch fault: advance directly to next instruction (fault handler)
        _status = BaseSimpleCPU::Running;
        advanceInst(fault);
    }

    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);
}


void
TimingSimpleCPU::advanceInst(const Fault &fault)
{
    SimpleExecContext &t_info = *threadInfo[curThread];

    if (_status == Faulting)
        return;

    if (fault != NoFault) {
        DPRINTF(SimpleCPU, "Fault occured. Handling the fault\n");

        advancePC(fault);

        // A syscall fault could suspend this CPU (e.g., futex_wait)
        // If the _status is not Idle, schedule an event to fetch the next
        // instruction after 'stall' ticks.
        // If the cpu has been suspended (i.e., _status == Idle), another
        // cpu will wake this cpu up later.
        if (_status != Idle) {
            DPRINTF(SimpleCPU, "Scheduling fetch event after the Fault\n");

            Tick stall = dynamic_pointer_cast<SyscallRetryFault>(fault) ?
                         clockEdge(syscallRetryLatency) : clockEdge();
            reschedule(fetchEvent, stall, true);
            _status = Faulting;
        }

        return;
    }

    if (!t_info.stayAtPC)
        advancePC(fault);

    if (tryCompleteDrain())
        return;

    if (_status == BaseSimpleCPU::Running) {
        // kick off fetch of next instruction... callback from icache
        // response will cause that instruction to be executed,
        // keeping the CPU running.
        fetch();
    }
}


void
TimingSimpleCPU::completeIfetch(PacketPtr pkt)
{
    SimpleExecContext& t_info = *threadInfo[curThread];

    DPRINTF(SimpleCPU, "Complete ICache Fetch for addr %#x\n", pkt ?
            pkt->getAddr() : 0);

    // received a response from the icache: execute the received
    // instruction
    assert(!pkt || !pkt->isError());
    assert(_status == IcacheWaitResponse);

    _status = BaseSimpleCPU::Running;

    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);

    if (pkt)
        pkt->req->setAccessLatency();


    preExecute();
    if (curStaticInst && curStaticInst->isMemRef()) {
        // load or store: just send to dcache
        Fault fault = curStaticInst->initiateAcc(&t_info, traceData);

        // If we're not running now the instruction will complete in a dcache
        // response callback or the instruction faulted and has started an
        // ifetch
        if (_status == BaseSimpleCPU::Running) {
            if (fault != NoFault && traceData) {
                // If there was a fault, we shouldn't trace this instruction.
                delete traceData;
                traceData = NULL;
            }

            postExecute();
            // @todo remove me after debugging with legion done
            if (curStaticInst && (!curStaticInst->isMicroop() ||
                        curStaticInst->isFirstMicroop()))
                instCnt++;
            advanceInst(fault);
        } else {
          dcache_start_cycle = curCycle();
        }
#ifdef BRG_SC3
    } else if (curStaticInst && curStaticInst->isMemBarrier()) {
        handleMemFence();
        // we're waiting for a response from dcache
        assert(_status != BaseSimpleCPU::Running);
        dcache_start_cycle = curCycle();
#endif
    } else if (curStaticInst) {
        // non-memory instruction: execute completely now
        Fault fault = curStaticInst->execute(&t_info, traceData);

        // keep an instruction count
        if (fault == NoFault)
            countInst();
        else if (traceData && !DTRACE(ExecFaulting)) {
            delete traceData;
            traceData = NULL;
        }

        postExecute();
        // @todo remove me after debugging with legion done
        if (curStaticInst && (!curStaticInst->isMicroop() ||
                curStaticInst->isFirstMicroop()))
            instCnt++;
        advanceInst(fault);
    } else {
        advanceInst(NoFault);
    }

    if (pkt) {
        delete pkt;
    }
}

void
TimingSimpleCPU::IcachePort::ITickEvent::process()
{
    cpu->completeIfetch(pkt);
}

bool
TimingSimpleCPU::IcachePort::recvTimingResp(PacketPtr pkt)
{
    DPRINTF(SimpleCPU, "Received fetch response %#x\n", pkt->getAddr());
    // we should only ever see one response per cycle since we only
    // issue a new request once this response is sunk
    assert(!tickEvent.scheduled());
    // delay processing of returned data until next CPU clock edge
    tickEvent.schedule(pkt, cpu->clockEdge());

#if THE_ISA == RISCV_ISA
    const Cycles delta(cpu->curCycle() - cpu->icache_start_cycle);
    if (cpu->in_runtime) {
        cpu->in_runtime_icache_cycles += delta;
        cpu->in_runtime_icache_num_ifetch += 1;
    } else {
        cpu->in_task_icache_cycles += delta;
        cpu->in_task_icache_num_ifetch += 1;
    }
    DPRINTF(SimpleCPU, "Request Type = ifetch, cycles = %lu\n",
            (uint64_t)delta);
#endif

    return true;
}

void
TimingSimpleCPU::IcachePort::recvReqRetry()
{
    // we shouldn't get a retry unless we have a packet that we're
    // waiting to transmit
    assert(cpu->ifetch_pkt != NULL);
    assert(cpu->_status == IcacheRetry);
    PacketPtr tmp = cpu->ifetch_pkt;
    if (sendTimingReq(tmp)) {
        cpu->_status = IcacheWaitResponse;
        cpu->ifetch_pkt = NULL;
    }
}

void
TimingSimpleCPU::completeDataAccess(PacketPtr pkt)
{
    // received a response from the dcache: complete the load or store
    // instruction
    assert(!pkt->isError());
    assert(_status == DcacheWaitResponse || _status == DTBWaitResponse ||
           pkt->req->getFlags().isSet(Request::NO_ACCESS));

    pkt->req->setAccessLatency();

    updateCycleCounts();
    updateCycleCounters(BaseCPU::CPU_STATE_ON);

    if (pkt->senderState) {
        SplitFragmentSenderState * send_state =
            dynamic_cast<SplitFragmentSenderState *>(pkt->senderState);
        assert(send_state);
        delete pkt;
        PacketPtr big_pkt = send_state->bigPkt;
        delete send_state;

        SplitMainSenderState * main_send_state =
            dynamic_cast<SplitMainSenderState *>(big_pkt->senderState);
        assert(main_send_state);
        // Record the fact that this packet is no longer outstanding.
        assert(main_send_state->outstanding != 0);
        main_send_state->outstanding--;

        if (main_send_state->outstanding) {
            return;
        } else {
            delete main_send_state;
            big_pkt->senderState = NULL;
            pkt = big_pkt;
        }
    }

    _status = BaseSimpleCPU::Running;

    Fault fault = NoFault;
    if (curStaticInst->isSyscall()) {
        assert(pkt->isMemFence());
        // after the fence, execute the syscall
        fault = curStaticInst->execute(threadInfo[curThread],
                                       traceData);
    } else if (!pkt->isMemFence()) {
        fault = curStaticInst->completeAcc(pkt, threadInfo[curThread],
                                           traceData);
    } else {
        // For memory fences, do nothing
    }

    // keep an instruction count
    if (fault == NoFault)
        countInst();
    else if (traceData) {
        // If there was a fault, we shouldn't trace this instruction.
        delete traceData;
        traceData = NULL;
    }

    delete pkt;

    postExecute();

    advanceInst(fault);
}

void
TimingSimpleCPU::updateCycleCounts()
{
    const Cycles delta(curCycle() - previousCycle);
    const Counter deltaInsts(totalInsts() - previousInsts);

    numCycles += delta;

    previousCycle = curCycle();
    previousInsts = totalInsts();

#if THE_ISA == RISCV_ISA
    if (in_runtime) {
        in_runtime_cycles += delta;
        in_runtime_insts += deltaInsts;
    } else {
        in_task_cycles += delta;
        in_task_insts += deltaInsts;
    }
#endif
}

void
TimingSimpleCPU::DcachePort::recvTimingSnoopReq(PacketPtr pkt)
{
    for (ThreadID tid = 0; tid < cpu->numThreads; tid++) {
        if (cpu->getCpuAddrMonitor(tid)->doMonitor(pkt)) {
            cpu->wakeup(tid);
        }
    }

    // Making it uniform across all CPUs:
    // The CPUs need to be woken up only on an invalidation packet (when using caches)
    // or on an incoming write packet (when not using caches)
    // It is not necessary to wake up the processor on all incoming packets
    if (pkt->isInvalidate() || pkt->isWrite()) {
        for (auto &t_info : cpu->threadInfo) {
            TheISA::handleLockedSnoop(t_info->thread, pkt, cacheBlockMask);
        }
    }
}

void
TimingSimpleCPU::DcachePort::recvFunctionalSnoop(PacketPtr pkt)
{
    for (ThreadID tid = 0; tid < cpu->numThreads; tid++) {
        if (cpu->getCpuAddrMonitor(tid)->doMonitor(pkt)) {
            cpu->wakeup(tid);
        }
    }
}

bool
TimingSimpleCPU::DcachePort::recvTimingResp(PacketPtr pkt)
{
    DPRINTF(SimpleCPU, "Received load/store response %#x\n", pkt->getAddr());

#if THE_ISA == RISCV_ISA
    const Cycles delta(cpu->curCycle() - cpu->dcache_start_cycle);
    if (pkt->isMemFence()) {
        if (cpu->in_runtime) {
            cpu->in_runtime_dcache_flush_cycles += delta;
            cpu->in_runtime_dcache_num_flush += 1;
        } else {
            cpu->in_task_dcache_flush_cycles += delta;
            cpu->in_task_dcache_num_flush += 1;
        }
        DPRINTF(SimpleCPU, "Request Type = flush, cycles = %lu\n",
                (uint64_t)delta);
    } else if (pkt->isLLSC()) {
        // LLSC
        if (pkt->isWrite()) {
            if (cpu->in_runtime) {
                cpu->in_runtime_dcache_sc_cycles += delta;
                cpu->in_runtime_dcache_num_sc += 1;
            } else {
                cpu->in_task_dcache_sc_cycles += delta;
                cpu->in_task_dcache_num_sc += 1;
            }
        } else {
            if (cpu->in_runtime) {
                cpu->in_runtime_dcache_ll_cycles += delta;
                cpu->in_runtime_dcache_num_ll += 1;
            } else {
                cpu->in_task_dcache_ll_cycles += delta;
                cpu->in_task_dcache_num_ll += 1;
            }
        }
    } else if (pkt->isAtomicOp()) {
        // AMO
        if (cpu->in_runtime) {
            cpu->in_runtime_dcache_amo_cycles += delta;
            cpu->in_runtime_dcache_num_amo += 1;
        } else {
            cpu->in_task_dcache_amo_cycles += delta;
            cpu->in_task_dcache_num_amo += 1;
        }
        DPRINTF(SimpleCPU, "Request Type = amo,   cycles = %lu\n",
                (uint64_t)delta);
    } else if (pkt->isWrite()) {
        // store
        if (cpu->in_runtime) {
            cpu->in_runtime_dcache_store_cycles += delta;
            cpu->in_runtime_dcache_num_store += 1;
            if (pkt->isCacheHit()) {
                cpu->in_runtime_dcache_store_hit_cycles += delta;
                cpu->in_runtime_dcache_num_store_hit += 1;
            } else {
                cpu->in_runtime_dcache_store_miss_cycles += delta;
                cpu->in_runtime_dcache_num_store_miss += 1;
            }
        } else {
            cpu->in_task_dcache_store_cycles += delta;
            cpu->in_task_dcache_num_store += 1;
            if (pkt->isCacheHit()) {
                cpu->in_task_dcache_store_hit_cycles += delta;
                cpu->in_task_dcache_num_store_hit += 1;
            } else {
                cpu->in_task_dcache_store_miss_cycles += delta;
                cpu->in_task_dcache_num_store_miss += 1;
            }
        }
        DPRINTF(SimpleCPU, "Request Type = store, cycles = %lu, hit = %s\n",
                (uint64_t)delta, pkt->isCacheHit() ? "yes" : "no");
    } else if (pkt->isRead()) {
        // load
        if (cpu->in_runtime) {
            cpu->in_runtime_dcache_load_cycles += delta;
            cpu->in_runtime_dcache_num_load += 1;
            if (pkt->isCacheHit()) {
                cpu->in_runtime_dcache_load_hit_cycles += delta;
                cpu->in_runtime_dcache_num_load_hit += 1;
            } else {
                cpu->in_runtime_dcache_load_miss_cycles += delta;
                cpu->in_runtime_dcache_num_load_miss += 1;
            }
        } else {
            cpu->in_task_dcache_load_cycles += delta;
            cpu->in_task_dcache_num_load += 1;
            if (pkt->isCacheHit()) {
                cpu->in_task_dcache_load_hit_cycles += delta;
                cpu->in_task_dcache_num_load_hit += 1;
            } else {
                cpu->in_task_dcache_load_miss_cycles += delta;
                cpu->in_task_dcache_num_load_miss += 1;
            }
        }
        DPRINTF(SimpleCPU, "Request Type = load,  cycles = %lu, hit = %s\n",
                (uint64_t)delta, pkt->isCacheHit() ? "yes" : "no");
    }

    if (cpu->in_runtime) {
        cpu->in_runtime_dcache_cycles += delta;
        cpu->in_runtime_dcache_num_access += 1;
    } else {
        cpu->in_task_dcache_cycles += delta;
        cpu->in_task_dcache_num_access += 1;
    }

#endif

    // The timing CPU is not really ticked, instead it relies on the
    // memory system (fetch and load/store) to set the pace.
    if (!tickEvent.scheduled()) {
        // Delay processing of returned data until next CPU clock edge
        tickEvent.schedule(pkt, cpu->clockEdge());
        return true;
    } else {
        // In the case of a split transaction and a cache that is
        // faster than a CPU we could get two responses in the
        // same tick, delay the second one
        if (!retryRespEvent.scheduled())
            cpu->schedule(retryRespEvent, cpu->clockEdge(Cycles(1)));
        return false;
    }
}

void
TimingSimpleCPU::DcachePort::DTickEvent::process()
{
    cpu->completeDataAccess(pkt);
}

void
TimingSimpleCPU::DcachePort::recvReqRetry()
{
    // we shouldn't get a retry unless we have a packet that we're
    // waiting to transmit
    assert(cpu->dcache_pkt != NULL);
    assert(cpu->_status == DcacheRetry);
    PacketPtr tmp = cpu->dcache_pkt;
    if (tmp->senderState) {
        // This is a packet from a split access.
        SplitFragmentSenderState * send_state =
            dynamic_cast<SplitFragmentSenderState *>(tmp->senderState);
        assert(send_state);
        PacketPtr big_pkt = send_state->bigPkt;

        SplitMainSenderState * main_send_state =
            dynamic_cast<SplitMainSenderState *>(big_pkt->senderState);
        assert(main_send_state);

        if (sendTimingReq(tmp)) {
            // If we were able to send without retrying, record that fact
            // and try sending the other fragment.
            send_state->clearFromParent();
            int other_index = main_send_state->getPendingFragment();
            if (other_index > 0) {
                tmp = main_send_state->fragments[other_index];
                cpu->dcache_pkt = tmp;
                if ((big_pkt->isRead() && cpu->handleReadPacket(tmp)) ||
                        (big_pkt->isWrite() && cpu->handleWritePacket())) {
                    main_send_state->fragments[other_index] = NULL;
                }
            } else {
                cpu->_status = DcacheWaitResponse;
                // memory system takes ownership of packet
                cpu->dcache_pkt = NULL;
            }
        }
    } else if (sendTimingReq(tmp)) {
        cpu->_status = DcacheWaitResponse;
        // memory system takes ownership of packet
        cpu->dcache_pkt = NULL;
    }
}

bool
TimingSimpleCPU::OutReqPort::recvTimingResp(PacketPtr pkt)
{
    assert(!tickEvent.scheduled());
    // Delay processing of returned data until next CPU clock edge
    tickEvent.schedule(pkt, cpu->clockEdge());
    return true;
}

void
TimingSimpleCPU::OutReqPort::AMTickEvent::process()
{
    cpu->recvULIResp(pkt);
}

bool
TimingSimpleCPU::InReqPort::recvTimingReq(PacketPtr pkt)
{
    if (!this->pendingPkt && cpu->recvULIReq(pkt)) {
        this->pendingPkt = pkt;
        return true;
    } else {
        // if recvULIReq (defined in BaseCPU) returns false,
        // the NetworkAdapter sends a NACK.
        return false;
    }
}

TimingSimpleCPU::IprEvent::IprEvent(Packet *_pkt, TimingSimpleCPU *_cpu,
    Tick t)
    : pkt(_pkt), cpu(_cpu)
{
    cpu->schedule(this, t);
}

void
TimingSimpleCPU::IprEvent::process()
{
    cpu->completeDataAccess(pkt);
}

const char *
TimingSimpleCPU::IprEvent::description() const
{
    return "Timing Simple CPU Delay IPR event";
}

void
TimingSimpleCPU::printAddr(Addr a)
{
    dcachePort.printAddr(a);
}

// moyang TC3-ULI
#if THE_ISA == RISCV_ISA

void
TimingSimpleCPU::sendULIReq()
{
    SimpleExecContext& t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    uint64_t target_id =
        thread->readMiscReg(TheISA::MISCREG_VALUE18);

    DPRINTF(ULI, "Send an ULI to cpu %lu\n", target_id);
    uli_req_sent++;
    auto pkt = new ActiveMsgPacket(cpuId(), target_id, 0, 1);
    pkt->setActiveMsgCmd(ActiveMsgPacket::ActiveMsgCmd::Get);
    assert(outReqPort.sendTimingReq(pkt));
}

void
TimingSimpleCPU::sendULIResp()
{
    const int RISCV_INTERRUPT_NUM_USI = 0; // User-level software interrupt
    assert(inReqPort.pendingPkt != nullptr);
    ActiveMsgPacket* pkt
        = dynamic_cast<ActiveMsgPacket*>(inReqPort.pendingPkt);
    assert(pkt);
    assert(pkt->isRequest());

    SimpleExecContext& t_info = *threadInfo[curThread];
    SimpleThread* thread = t_info.thread;

    // the original sender becomes the receiver
    uint64_t resp_id = pkt->getSenderId();
    uint64_t payload = thread->readMiscReg(TheISA::MISCREG_VALUE19);

    pkt->makeResponse();

    if (payload != 0) {
        pkt->setActiveMsgCmd(ActiveMsgPacket::ActiveMsgCmd::Ack);
        pkt->setPayload(payload);
        uli_resp_ack_sent++;
    } else {
        pkt->setActiveMsgCmd(ActiveMsgPacket::ActiveMsgCmd::Nack);
        uli_resp_nack_sent++;
    }

    // clear pending interrupt
    int tid = 0, num = RISCV_INTERRUPT_NUM_USI, idx = 0;
    clearInterrupt(tid, num, idx);

    if (inReqPort.sendTimingResp(pkt)) {
        DPRINTF(ULI, "Sent an ULI response to %lu successfully, "
                "val = %lu\n",
                resp_id, payload);
        inReqPort.pendingPkt = nullptr;
    }
}

void
TimingSimpleCPU::sendULIRespNack()
{
    const int RISCV_INTERRUPT_NUM_USI = 0; // User-level software interrupt
    assert(inReqPort.pendingPkt != nullptr);
    ActiveMsgPacket* pkt
        = dynamic_cast<ActiveMsgPacket*>(inReqPort.pendingPkt);
    assert(pkt);
    assert(pkt->isRequest());

    // the original sender becomes the receiver
    uint64_t resp_id = pkt->getSenderId();

    pkt->makeResponse();
    DPRINTF(ULI, "sendULIRespNack() called due to ULI cleared\n");
    pkt->setActiveMsgCmd(ActiveMsgPacket::ActiveMsgCmd::Nack);
    uli_resp_nack_sent++;

    // clear pending interrupt
    int tid = 0, num = RISCV_INTERRUPT_NUM_USI, idx = 0;
    clearInterrupt(tid, num, idx);

    if (inReqPort.sendTimingResp(pkt)) {
        DPRINTF(ULI, "Sent an ULI response to %lu successfully, "
                "val = 0\n", resp_id);
        inReqPort.pendingPkt = nullptr;
    }
}

void
TimingSimpleCPU::InReqPort::recvRespRetry()
{
    assert(pendingPkt);
    assert(pendingPkt->isResponse());
    ActiveMsgPacket* pkt_a
        = dynamic_cast<ActiveMsgPacket*>(pendingPkt);
    assert(pkt_a);
    assert(pkt_a->getActiveMsgCmd() == ActiveMsgPacket::ActiveMsgCmd::Ack
           || pkt_a->getActiveMsgCmd() == ActiveMsgPacket::ActiveMsgCmd::Nack);

    if (sendTimingResp(pendingPkt)) {
        DPRINTF(SimpleCPU, "Retry sending an active msg response success,"
                "dst = %lu, payload = %lu\n",
                pkt_a->getReceiverId(),
                pkt_a->getPayload());
        pendingPkt = nullptr;
    }
}

#endif

// appl stats
#if THE_ISA == RISCV_ISA
void
TimingSimpleCPU::toggle_stats_en(bool on)
{
    if (on) {
       applRuntimeStartCycle = curCycle();
    }
}

void
TimingSimpleCPU::toggleCSR(int misc_reg, RegVal reg_val)
{
    if (misc_reg == TheISA::MISCREG_TOGGLE1) {
        if (reg_val == 1) {
            in_runtime = true;
        } else {
            in_runtime = false;
        }
    }

    if (!global_stats_en) {
        // do not toggle if not in ROI
        return;
    }
    switch (misc_reg) {
        case TheISA::MISCREG_TOGGLE1: {
            if (reg_val == 1) {
                applRuntimeStartCycle = curCycle();
            } else if (reg_val == 0 && (uint64_t)applRuntimeStartCycle != 0) {
                const Cycles delta(curCycle() - applRuntimeStartCycle);
                appl_runtime_cycles += delta;
            }
            break;
        }
        case TheISA::MISCREG_TOGGLE2: {
            if (reg_val == 1) {
                applEnqueueStartCycle = curCycle();
            } else if (reg_val == 0 && (uint64_t)applEnqueueStartCycle != 0) {
                const Cycles delta(curCycle() - applEnqueueStartCycle);
                appl_enqueue_cycles += delta;
                appl_num_enqueue += 1;
            }
            break;
        }
        case TheISA::MISCREG_TOGGLE3: {
            if (reg_val == 1) {
                applDequeueStartCycle = curCycle();
            } else if (reg_val == 0 && (uint64_t)applDequeueStartCycle != 0) {
                const Cycles delta(curCycle() - applDequeueStartCycle);
                appl_dequeue_cycles += delta;
                appl_num_dequeue += 1;
            }
            break;
        }
        case TheISA::MISCREG_TOGGLE4: {
            if (reg_val == 1) {
                applStealStartCycle = curCycle();
            } else if (reg_val == 0 && applStealStartCycle != 0) {
                const Cycles delta(curCycle() - applStealStartCycle);
                appl_steal_successful_cycles += delta;
                appl_num_steal_successful += 1;
            }
            break;
        }
        case TheISA::MISCREG_TOGGLE5: {
            if (reg_val == 1) {
                applStealStartCycle = curCycle();
            } else if (reg_val == 0 && applStealStartCycle != 0) {
                const Cycles delta(curCycle() - applStealStartCycle);
                appl_steal_failed_cycles += delta;
                appl_num_steal_failed += 1;
            }
            break;
        }
       case TheISA::MISCREG_TOGGLE6: {
            if (reg_val == 0) {
                appl_num_execute += 1;
            }
            break;
        }
    }
}

void
TimingSimpleCPU::regStats()
{
    BaseSimpleCPU::regStats();
    using namespace Stats;

    num_active_message_ack
        .name(name() + ".num_active_message_ack")
        .desc("number of active messages ACK received");

    active_message_ack_latency
        .name(name() + ".active_message_ack_latency")
        .desc("active messages ACK latency");

    num_active_message_nack
        .name(name() + ".num_active_message_nack")
        .desc("number of active messages NACK received");

    active_message_nack_latency
        .name(name() + ".active_message_nack_latency")
        .desc("active messages NACK latency");

    appl_num_steal_successful
        .name(name() + ".appl_num_steal_successful")
        .desc("number of successful steals")
        ;

    appl_steal_successful_cycles
        .name(name() + ".appl_steal_successful_cycles")
        .desc("cycles of successful steals")
        ;

    appl_num_steal_failed
        .name(name() + ".appl_num_steal_failed")
        .desc("number of failed steals")
        ;

    appl_steal_failed_cycles
        .name(name() + ".appl_steal_failed_cycles")
        .desc("cycles of failed steals")
        ;

    appl_runtime_cycles
        .name(name() + ".appl_runtime_cycles")
        .desc("number of cycles spent in appl runtime")
        ;

    appl_num_enqueue
        .name(name() + ".appl_num_enqueue")
        .desc("number of appl enqueue")
        ;

    appl_enqueue_cycles
        .name(name() + ".appl_enqueue_cycles")
        .desc("number of cycles spent in appl enqueue")
        ;

    appl_num_dequeue
        .name(name() + ".appl_num_dequeue")
        .desc("number of appl dequeue")
        ;

    appl_dequeue_cycles
        .name(name() + ".appl_dequeue_cycles")
        .desc("number of cycles spent in appl dequeue")
        ;

    appl_num_execute
        .name(name() + ".appl_num_execute")
        .desc("number of appl execute")
        ;

    appl_num_self_invalidation
        .name(name() + ".appl_num_self_invalidation")
        .desc("number of self invalidation")
        ;

    appl_num_self_writeback
        .name(name() + ".appl_num_self_writeback")
        .desc("number of self writeback")
        ;

    in_task_cycles
        .name(name() + ".in_task_cycles" );

    in_task_insts
        .name(name() + ".in_task_insts" );

    in_task_icache_cycles
        .name(name() + ".in_task_icache_cycles");

    in_task_dcache_cycles
        .name(name() + ".in_task_dcache_cycles");

    in_task_dcache_load_cycles
        .name(name() + ".in_task_dcache_load_cycles");

    in_task_dcache_load_hit_cycles
        .name(name() + ".in_task_dcache_load_hit_cycles");

    in_task_dcache_load_miss_cycles
        .name(name() + ".in_task_dcache_load_miss_cycles");

    in_task_dcache_store_cycles
        .name(name() + ".in_task_dcache_store_cycles");

    in_task_dcache_store_hit_cycles
        .name(name() + ".in_task_dcache_store_hit_cycles");

    in_task_dcache_store_miss_cycles
        .name(name() + ".in_task_dcache_store_miss_cycles");

    in_task_dcache_amo_cycles
        .name(name() + ".in_task_dcache_amo_cycles");

    in_task_dcache_ll_cycles
        .name(name() + ".in_task_dcache_ll_cycles");

    in_task_dcache_sc_cycles
        .name(name() + ".in_task_dcache_sc_cycles");

    in_task_dcache_flush_cycles
        .name(name() + ".in_task_dcache_flush_cycles");

    in_task_icache_num_ifetch
        .name(name() + ".in_task_icache_num_ifetch");

    in_task_dcache_num_access
        .name(name() + ".in_task_dcache_num_access");

    in_task_dcache_num_load
        .name(name() + ".in_task_dcache_num_load");

    in_task_dcache_num_load_hit
        .name(name() + ".in_task_dcache_num_load_hit");

    in_task_dcache_num_load_miss
        .name(name() + ".in_task_dcache_num_load_miss");

    in_task_dcache_num_store
        .name(name() + ".in_task_dcache_num_store");

    in_task_dcache_num_store_hit
        .name(name() + ".in_task_dcache_num_store_hit");

    in_task_dcache_num_store_miss
        .name(name() + ".in_task_dcache_num_store_miss");

    in_task_dcache_num_amo
        .name(name() + ".in_task_dcache_num_amo");

    in_task_dcache_num_ll
        .name(name() + ".in_task_dcache_num_ll");

    in_task_dcache_num_sc
        .name(name() + ".in_task_dcache_num_sc");

    in_task_dcache_num_flush
        .name(name() + ".in_task_dcache_num_flush");

    in_runtime_cycles
        .name(name() + ".in_runtime_cycles");

    in_runtime_insts
        .name(name() + ".in_runtime_insts");

    in_runtime_icache_cycles
        .name(name() + ".in_runtime_icache_cycles");

    in_runtime_dcache_cycles
        .name(name() + ".in_runtime_dcache_cycles");

    in_runtime_dcache_load_cycles
        .name(name() + ".in_runtime_dcache_load_cycles");

    in_runtime_dcache_load_hit_cycles
        .name(name() + ".in_runtime_dcache_load_hit_cycles");

    in_runtime_dcache_load_miss_cycles
        .name(name() + ".in_runtime_dcache_load_miss_cycles");

    in_runtime_dcache_store_cycles
        .name(name() + ".in_runtime_dcache_store_cycles");

    in_runtime_dcache_store_hit_cycles
        .name(name() + ".in_runtime_dcache_store_hit_cycles");

    in_runtime_dcache_store_miss_cycles
        .name(name() + ".in_runtime_dcache_store_miss_cycles");

    in_runtime_dcache_amo_cycles
        .name(name() + ".in_runtime_dcache_amo_cycles");

    in_runtime_dcache_ll_cycles
        .name(name() + ".in_runtime_dcache_ll_cycles");

    in_runtime_dcache_sc_cycles
        .name(name() + ".in_runtime_dcache_sc_cycles");

    in_runtime_dcache_flush_cycles
        .name(name() + ".in_runtime_dcache_flush_cycles");

    in_runtime_icache_num_ifetch
        .name(name() + ".in_runtime_icache_num_ifetch");

    in_runtime_dcache_num_access
        .name(name() + ".in_runtime_dcache_num_access");

    in_runtime_dcache_num_load
        .name(name() + ".in_runtime_dcache_num_load");

    in_runtime_dcache_num_load_hit
        .name(name() + ".in_runtime_dcache_num_load_hit");

    in_runtime_dcache_num_load_miss
        .name(name() + ".in_runtime_dcache_num_load_miss");

    in_runtime_dcache_num_store
        .name(name() + ".in_runtime_dcache_num_store");

    in_runtime_dcache_num_store_hit
        .name(name() + ".in_runtime_dcache_num_store_hit");

    in_runtime_dcache_num_store_miss
        .name(name() + ".in_runtime_dcache_num_store_miss");

    in_runtime_dcache_num_amo
        .name(name() + ".in_runtime_dcache_num_amo");

    in_runtime_dcache_num_ll
        .name(name() + ".in_runtime_dcache_num_ll");

    in_runtime_dcache_num_sc
        .name(name() + ".in_runtime_dcache_num_sc");

    in_runtime_dcache_num_flush
        .name(name() + ".in_runtime_dcache_num_flush");
}
#endif

////////////////////////////////////////////////////////////////////////
//
//  TimingSimpleCPU Simulation Object
//
TimingSimpleCPU *
TimingSimpleCPUParams::create()
{
    return new TimingSimpleCPU(this);
}

