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
 * Copyright (c) 2011-2012, 2014, 2016, 2017, 2019 ARM Limited
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
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
 * Copyright (c) 2004-2006 The Regents of The University of Michigan
 * Copyright (c) 2011 Regents of the University of California
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
 * Authors: Kevin Lim
 *          Korey Sewell
 *          Rick Strong
 */

#include "cpu/o3/cpu.hh"

#include "arch/generic/traits.hh"
#include "arch/kernel_stats.hh"
#include "config/the_isa.hh"
#include "cpu/activity.hh"
#include "cpu/checker/cpu.hh"
#include "cpu/checker/thread_context.hh"
#include "cpu/o3/isa_specific.hh"
#include "cpu/o3/thread_context.hh"
#include "cpu/quiesce_event.hh"
#include "cpu/simple_thread.hh"
#include "cpu/thread_context.hh"
#include "debug/Activity.hh"
#include "debug/Drain.hh"
#include "debug/O3CPU.hh"
#include "debug/Quiesce.hh"
#include "debug/ULI.hh"
#include "enums/MemoryMode.hh"
#include "sim/core.hh"
#include "sim/full_system.hh"
#include "sim/line_trace.hh"
#include "sim/process.hh"
#include "sim/stat_control.hh"
#include "sim/system.hh"

#if THE_ISA == ALPHA_ISA
#include "arch/alpha/osfpal.hh"
#include "debug/Activity.hh"

#endif

struct BaseCPUParams;

using namespace TheISA;
using namespace std;

BaseO3CPU::BaseO3CPU(BaseCPUParams *params)
    : BaseCPU(params)
{
}

void
BaseO3CPU::regStats()
{
    BaseCPU::regStats();
}

template<class Impl>
bool
FullO3CPU<Impl>::IcachePort::sendTimingReq(PacketPtr pkt)
{
    assert(numUsedCachePorts <= numCachePorts);
    if (numUsedCachePorts == numCachePorts) {
        // DPRINTF(O3CPU, "Run out of cache ports\n");
        needRetry = true;
        return false;
    }

    if (MasterPort::sendTimingReq(pkt)) {
        numUsedCachePorts++;
        return true;
    }

    return false;
}

template<class Impl>
bool
FullO3CPU<Impl>::IcachePort::recvTimingResp(PacketPtr pkt)
{
    // DPRINTF(O3CPU, "Fetch unit received timing\n");
    // We shouldn't ever get a cacheable block in Modified state
    assert(pkt->req->isUncacheable() ||
           !(pkt->cacheResponding() && !pkt->hasSharers()));
    fetch->processCacheCompletion(pkt);

    return true;
}

template<class Impl>
void
FullO3CPU<Impl>::IcachePort::recvReqRetry()
{
    fetch->recvReqRetry();
}

template<class Impl>
void
FullO3CPU<Impl>::IcachePort::reset()
{
    numUsedCachePorts = 0;
    if (needRetry) {
        needRetry = false;
        fetch->recvReqRetry();
    }
}

template<class Impl>
bool
FullO3CPU<Impl>::DcachePort::sendTimingReq(PacketPtr pkt)
{
    assert(numUsedCachePorts <= numCachePorts);
    if (numUsedCachePorts == numCachePorts) {
        // DPRINTF(O3CPU, "Run out of cache ports\n");
        needRetry = true;
        return false;
    }

    if (MasterPort::sendTimingReq(pkt)) {
        numUsedCachePorts++;
        return true;
    }

    return false;
}

template <class Impl>
bool
FullO3CPU<Impl>::DcachePort::recvTimingResp(PacketPtr pkt)
{
    // DPRINTF(O3CPU, "LSQ received timing resp\n");
    return lsq->recvTimingResp(pkt);
}

template <class Impl>
void
FullO3CPU<Impl>::DcachePort::recvTimingSnoopReq(PacketPtr pkt)
{
    for (ThreadID tid = 0; tid < cpu->numThreads; tid++) {
        if (cpu->getCpuAddrMonitor(tid)->doMonitor(pkt)) {
            cpu->wakeup(tid);
        }
    }
    lsq->recvTimingSnoopReq(pkt);
}

template <class Impl>
void
FullO3CPU<Impl>::DcachePort::recvReqRetry()
{
    lsq->recvReqRetry();
}

template<class Impl>
void
FullO3CPU<Impl>::DcachePort::reset()
{
    numUsedCachePorts = 0;
    if (needRetry) {
        needRetry = false;
        lsq->recvReqRetry();
    }
}

template<class Impl>
bool
FullO3CPU<Impl>::OutReqPort::recvTimingResp(PacketPtr pkt)
{
    // Defined in BaseCPU, Set certain CSRs on this cpu
    cpu->recvULIResp(pkt);
    return true;
}

template<class Impl>
bool
FullO3CPU<Impl>::InReqPort::recvTimingReq(PacketPtr pkt)
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

// moyang TC3-ULI
#if THE_ISA == RISCV_ISA

template<class Impl>
void
FullO3CPU<Impl>::setNoSquashFromTC(bool on)
{
    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        assert(thread[tid]->noSquashFromTC != on);
        thread[tid]->noSquashFromTC = on;
    }
}

template<class Impl>
void
FullO3CPU<Impl>::sendULIReq()
{
    // only allow single thread
    assert(threadContexts.size() == 1);
    assert(numThreads == 1);
    ThreadContext* tc = getContext(0);
    assert(tc == tcBase(0));

    uint64_t target_id =
        tc->readMiscReg(TheISA::MISCREG_VALUE18);

    DPRINTF(ULI, "Send an ULI to cpu %lu\n", target_id);
    uli_req_sent++;

    auto pkt = new ActiveMsgPacket(cpuId(), target_id, 0, 1);
    pkt->setActiveMsgCmd(ActiveMsgPacket::ActiveMsgCmd::Get);
    assert(outReqPort.sendTimingReq(pkt));
}

template<class Impl>
void
FullO3CPU<Impl>::sendULIResp()
{
    const int RISCV_INTERRUPT_NUM_USI = 0; // User-level software interrupt

    // only allow single thread
    assert(numThreads == 1);
    ThreadContext* tc = tcBase(0);

    assert(inReqPort.pendingPkt != nullptr);
    ActiveMsgPacket* pkt
        = dynamic_cast<ActiveMsgPacket*>(inReqPort.pendingPkt);
    assert(pkt);
    assert(pkt->isRequest());

    // the original sender becomes the receiver
    uint64_t resp_id = pkt->getSenderId();
    uint64_t payload = tc->readMiscReg(TheISA::MISCREG_VALUE19);

    pkt->makeResponse();

    bool ack = false;
    if (payload != 0) {
        pkt->setActiveMsgCmd(ActiveMsgPacket::ActiveMsgCmd::Ack);
        pkt->setPayload(payload);
        ack = true;
    } else {
        pkt->setActiveMsgCmd(ActiveMsgPacket::ActiveMsgCmd::Nack);
    }

    DPRINTF(ULI, "sendULIResp() called, type = %s, resp_id = %ld, "
                 "payload = %ld\n", ack ? "Ack" : "Nack", resp_id,
                 payload);
    if (ack) uli_resp_ack_sent++;
    else uli_resp_nack_sent++;

    // clear pending interrupt
    int tid = 0, num = RISCV_INTERRUPT_NUM_USI, idx = 0;
    clearInterrupt(tid, num, idx);

    if (inReqPort.sendTimingResp(pkt)) {
        DPRINTF(ULI, "Sent an ULI response to %lu successfully.\n",
                resp_id);
        inReqPort.pendingPkt = nullptr;
    }
}

template<class Impl>
void
FullO3CPU<Impl>::sendULIRespNack()
{
    if (!inReqPort.pendingPkt) {
        DPRINTF(ULI, "Nothing to clear.\n");
        return;
    }

    const int RISCV_INTERRUPT_NUM_USI = 0; // User-level software interrupt

    // only allow single thread
    assert(numThreads == 1);
    assert(inReqPort.pendingPkt != nullptr);
    ActiveMsgPacket* pkt
        = dynamic_cast<ActiveMsgPacket*>(inReqPort.pendingPkt);
    assert(pkt);
    assert(pkt->isRequest());

    // the original sender becomes the receiver
    uint64_t resp_id = pkt->getSenderId();

    pkt->makeResponse();
    pkt->setActiveMsgCmd(ActiveMsgPacket::ActiveMsgCmd::Nack);

    DPRINTF(ULI, "sendULIRespNack() called due to ULI cleared\n");
    uli_resp_nack_sent++;

    // clear pending interrupt
    int tid = 0, num = RISCV_INTERRUPT_NUM_USI, idx = 0;
    clearInterrupt(tid, num, idx);

    if (inReqPort.sendTimingResp(pkt)) {
        DPRINTF(ULI, "Sent an ULI response to %lu successfully.\n",
                resp_id);
        inReqPort.pendingPkt = nullptr;
    }
}

template<class Impl>
void
FullO3CPU<Impl>::InReqPort::recvRespRetry()
{
    assert(pendingPkt);
    assert(pendingPkt->isResponse());
    ActiveMsgPacket* pkt_a
        = dynamic_cast<ActiveMsgPacket*>(pendingPkt);
    assert(pkt_a);
    assert(pkt_a->getActiveMsgCmd() == ActiveMsgPacket::ActiveMsgCmd::Ack
           || pkt_a->getActiveMsgCmd() == ActiveMsgPacket::ActiveMsgCmd::Nack);

    if (sendTimingResp(pendingPkt)) {
        DPRINTF(ULI, "Retry sending an active msg response success,"
                "dst = %lu, payload = %lu\n",
                pkt_a->getReceiverId(),
                pkt_a->getPayload());
        pendingPkt = nullptr;
    }
}

#endif

template <class Impl>
FullO3CPU<Impl>::FullO3CPU(DerivO3CPUParams *params)
    : BaseO3CPU(params),
      itb(params->itb),
      dtb(params->dtb),
      tickEvent([this]{ tick(); }, "FullO3CPU tick",
                false, Event::CPU_Tick_Pri),
      threadExitEvent([this]{ exitThreads(); }, "FullO3CPU exit threads",
                false, Event::CPU_Exit_Pri),
#ifndef NDEBUG
      instcount(0),
#endif
      removeInstsThisCycle(false),
      fetch(this, params),
      decode(this, params),
      rename(this, params),
      iew(this, params),
      commit(this, params),

      /* It is mandatory that all SMT threads use the same renaming mode as
       * they are sharing registers and rename */
      vecMode(RenameMode<TheISA::ISA>::init(params->isa[0])),
      regFile(params->numPhysIntRegs,
              params->numPhysFloatRegs,
              params->numPhysVecRegs,
              params->numPhysVecPredRegs,
              params->numPhysCCRegs,
              vecMode),

      freeList(name() + ".freelist", &regFile),

      rob(this, params),

      scoreboard(name() + ".scoreboard",
                 regFile.totalNumPhysRegs()),

      isa(numThreads, NULL),

      icachePort(&fetch, this, params->numIcachePorts),
      dcachePort(&iew.ldstQueue, this, params->numDcachePorts),

      // moyang: TC3-ULI
      outReqPort(this, ".out_nw_req_port"),
      inReqPort(this, ".in_nw_req_port"),

      fetchQueue(params->backComSize, params->forwardComSize),
      decodeQueue(params->backComSize, params->forwardComSize),
      renameQueue(params->backComSize, params->forwardComSize),
      iewQueue(params->backComSize, params->forwardComSize),
      activityRec(name(), NumStages,
                  params->backComSize + params->forwardComSize,
                  params->activity),

      globalSeqNum(1),
      system(params->system),
      lastRunningCycle(curCycle())
{
    if (!params->switched_out) {
        _status = Running;
    } else {
        _status = SwitchedOut;
    }

    if (params->checker) {
        BaseCPU *temp_checker = params->checker;
        checker = dynamic_cast<Checker<Impl> *>(temp_checker);
        checker->setIcachePort(&icachePort);
        checker->setSystem(params->system);
    } else {
        checker = NULL;
    }

    if (!FullSystem) {
        thread.resize(numThreads);
        tids.resize(numThreads);
    }

    // set up timeBuffer and forwardTimeBuffer for all threads
    timeBuffer.reserve(numThreads);
    forwardTimeBuffer.reserve(numThreads);

    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        timeBuffer.emplace_back(params->backComSize, params->forwardComSize);
        forwardTimeBuffer.emplace_back(params->backComSize,
                                       params->forwardComSize);
    }

    // The stages also need their CPU pointer setup.  However this
    // must be done at the upper level CPU because they have pointers
    // to the upper level CPU, and not this FullO3CPU.

    // Set up Pointers to the activeThreads list for each stage
    fetch.setActiveThreads(&activeThreads);
    decode.setActiveThreads(&activeThreads);
    rename.setActiveThreads(&activeThreads);
    iew.setActiveThreads(&activeThreads);
    commit.setActiveThreads(&activeThreads);

    // Give each of the stages the time buffer they will use.
    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        fetch.setTimeBuffer(&timeBuffer[tid], tid);
        decode.setTimeBuffer(&timeBuffer[tid], tid);
        rename.setTimeBuffer(&timeBuffer[tid], tid);
        iew.setTimeBuffer(&timeBuffer[tid], tid);
        commit.setTimeBuffer(&timeBuffer[tid], tid);

        iew.setForwardTimeBuffer(&forwardTimeBuffer[tid], tid);
        commit.setForwardTimeBuffer(&forwardTimeBuffer[tid], tid);
    }

    // Also setup each of the stages' queues.
    fetch.setFetchQueue(&fetchQueue);
    decode.setFetchQueue(&fetchQueue);
    decode.setDecodeQueue(&decodeQueue);
    rename.setDecodeQueue(&decodeQueue);
    rename.setRenameQueue(&renameQueue);
    iew.setRenameQueue(&renameQueue);
    iew.setIEWQueue(&iewQueue);
    commit.setIEWQueue(&iewQueue);
    commit.setRenameQueue(&renameQueue);

    commit.setIEWStage(&iew);
    rename.setIEWStage(&iew);
    rename.setCommitStage(&commit);

    // Init haltingThreads and suspendingThreads
    haltingThreads.resize(numThreads, false);
    suspendingThreads.resize(numThreads, false);

    threadsToActivate.resize(numThreads, false);

    ThreadID active_threads;
    if (FullSystem) {
        active_threads = 1;
    } else if (numThreads > 1 && params->workload.size() == 1){
        // single workload/program & multiple threads
        active_threads = numThreads;
    } else {
        // either single workload/program & single thread,
        // or multiple workloads & multiple threads
        // The number of active threads is the number of workloads
        active_threads = params->workload.size();

        if (active_threads > Impl::MaxThreads) {
            panic("Workload Size too large. Increase the 'MaxThreads' "
                  "constant in your O3CPU impl. file (e.g. o3/alpha/impl.hh) "
                  "or edit your workload size.");
        }
    }

    //Make Sure That this a Valid Architeture
    assert(params->numPhysIntRegs   >= numThreads * TheISA::NumIntRegs);
    assert(params->numPhysFloatRegs >= numThreads * TheISA::NumFloatRegs);
    assert(params->numPhysVecRegs >= numThreads * TheISA::NumVecRegs);
    assert(params->numPhysVecPredRegs >= numThreads * TheISA::NumVecPredRegs);
    assert(params->numPhysCCRegs >= numThreads * TheISA::NumCCRegs);

    rename.setScoreboard(&scoreboard);
    iew.setScoreboard(&scoreboard);

    // Setup the rename map for whichever stages need it.
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        isa[tid] = params->isa[tid];
        assert(RenameMode<TheISA::ISA>::equalsInit(isa[tid], isa[0]));

        // Only Alpha has an FP zero register, so for other ISAs we
        // use an invalid FP register index to avoid special treatment
        // of any valid FP reg.
        RegIndex invalidFPReg = TheISA::NumFloatRegs + 1;
        RegIndex fpZeroReg =
            (THE_ISA == ALPHA_ISA) ? TheISA::ZeroReg : invalidFPReg;

        commitRenameMap[tid].init(&regFile, TheISA::ZeroReg, fpZeroReg,
                                  &freeList,
                                  vecMode);

        renameMap[tid].init(&regFile, TheISA::ZeroReg, fpZeroReg,
                            &freeList, vecMode);
    }

    // Initialize rename map to assign physical registers to the
    // architectural registers for active threads only.
    PhysRegIdPtr zero_int_phys_reg = nullptr;
    PhysRegIdPtr zero_float_phys_reg = nullptr;

    for (ThreadID tid = 0; tid < active_threads; tid++) {
        for (RegIndex ridx = 0; ridx < TheISA::NumIntRegs; ++ridx) {
            // check if ridx is ZeroReg and a physical register has already
            // been assigned to the zero register. If so, try to use the
            // existing mapping.
            PhysRegIdPtr phys_reg = nullptr;
            if (ridx == TheISA::ZeroReg && zero_int_phys_reg)
                phys_reg = zero_int_phys_reg;
            else
                phys_reg = freeList.getIntReg();

            renameMap[tid].setEntry(RegId(IntRegClass, ridx), phys_reg);
            commitRenameMap[tid].setEntry(RegId(IntRegClass, ridx), phys_reg);

            if (ridx == TheISA::ZeroReg && !zero_int_phys_reg)
                zero_int_phys_reg = phys_reg;
        }

        for (RegIndex ridx = 0; ridx < TheISA::NumFloatRegs; ++ridx) {
            // check if ridx is ZeroReg and a physical register has already
            // been assigned to the zero register. If so, try to use the
            // existing mapping.
            PhysRegIdPtr phys_reg = nullptr;
            if (THE_ISA == ALPHA_ISA && ridx == TheISA::ZeroReg &&
                zero_float_phys_reg)
                phys_reg = zero_float_phys_reg;
            else
                phys_reg = freeList.getFloatReg();

            renameMap[tid].setEntry(RegId(FloatRegClass, ridx), phys_reg);
            commitRenameMap[tid].setEntry(
                    RegId(FloatRegClass, ridx), phys_reg);

            if (THE_ISA == ALPHA_ISA && ridx == TheISA::ZeroReg &&
                !zero_float_phys_reg)
                zero_float_phys_reg = phys_reg;
        }

        /* Here we need two 'interfaces' the 'whole register' and the
         * 'register element'. At any point only one of them will be
         * active. */
        if (vecMode == Enums::Full) {
            /* Initialize the full-vector interface */
            for (RegIndex ridx = 0; ridx < TheISA::NumVecRegs; ++ridx) {
                RegId rid = RegId(VecRegClass, ridx);
                PhysRegIdPtr phys_reg = freeList.getVecReg();
                renameMap[tid].setEntry(rid, phys_reg);
                commitRenameMap[tid].setEntry(rid, phys_reg);
            }
        } else {
            /* Initialize the vector-element interface */
            for (RegIndex ridx = 0; ridx < TheISA::NumVecRegs; ++ridx) {
                for (ElemIndex ldx = 0; ldx < TheISA::NumVecElemPerVecReg;
                        ++ldx) {
                    RegId lrid = RegId(VecElemClass, ridx, ldx);
                    PhysRegIdPtr phys_elem = freeList.getVecElem();
                    renameMap[tid].setEntry(lrid, phys_elem);
                    commitRenameMap[tid].setEntry(lrid, phys_elem);
                }
            }
        }

        for (RegIndex ridx = 0; ridx < TheISA::NumVecPredRegs; ++ridx) {
            PhysRegIdPtr phys_reg = freeList.getVecPredReg();
            renameMap[tid].setEntry(RegId(VecPredRegClass, ridx), phys_reg);
            commitRenameMap[tid].setEntry(
                    RegId(VecPredRegClass, ridx), phys_reg);
        }

        for (RegIndex ridx = 0; ridx < TheISA::NumCCRegs; ++ridx) {
            PhysRegIdPtr phys_reg = freeList.getCCReg();
            renameMap[tid].setEntry(RegId(CCRegClass, ridx), phys_reg);
            commitRenameMap[tid].setEntry(RegId(CCRegClass, ridx), phys_reg);
        }
    }

    rename.setRenameMap(renameMap);
    commit.setRenameMap(commitRenameMap);
    rename.setFreeList(&freeList);

    // Setup the ROB for whichever stages need it.
    commit.setROB(&rob);

    lastActivatedCycle.resize(numThreads, 0);
#if 0
    // Give renameMap & rename stage access to the freeList;
    for (ThreadID tid = 0; tid < numThreads; tid++)
        globalSeqNum[tid] = 1;
#endif

    DPRINTF(O3CPU, "Creating O3CPU object.\n");

    // Setup any thread state.
    this->thread.resize(this->numThreads);

    for (ThreadID tid = 0; tid < this->numThreads; ++tid) {
        if (FullSystem) {
            // SMT is not supported in FS mode yet.
            assert(this->numThreads == 1);
            this->thread[tid] = new Thread(this, 0, NULL);
        } else if (numThreads > 1 && params->workload.size() == 1){
            // single workload/program & multiple threads
            // All threads are assigned to the same workload
            DPRINTF(O3CPU, "Thread [tid:%d] is assigned to workload 0\n", tid);
            this->thread[tid] = new typename FullO3CPU<Impl>::Thread(
                    (typename Impl::O3CPU *)(this),
                    tid, params->workload[0]);
        } else {
            if (tid < params->workload.size()) {
                DPRINTF(O3CPU, "Workload[%i] process is %#x",
                        tid, this->thread[tid]);
                this->thread[tid] = new typename FullO3CPU<Impl>::Thread(
                        (typename Impl::O3CPU *)(this),
                        tid, params->workload[tid]);

                //usedTids[tid] = true;
                //threadMap[tid] = tid;
            } else {
                //Allocate Empty thread so M5 can use later
                //when scheduling threads to CPU
                Process* dummy_proc = NULL;

                this->thread[tid] = new typename FullO3CPU<Impl>::Thread(
                        (typename Impl::O3CPU *)(this),
                        tid, dummy_proc);
                //usedTids[tid] = false;
            }
        }

        ThreadContext *tc;

        // Setup the TC that will serve as the interface to the threads/CPU.
        O3ThreadContext<Impl> *o3_tc = new O3ThreadContext<Impl>;

        tc = o3_tc;

        // If we're using a checker, then the TC should be the
        // CheckerThreadContext.
        if (params->checker) {
            tc = new CheckerThreadContext<O3ThreadContext<Impl> >(
                o3_tc, this->checker);
        }

        o3_tc->cpu = (typename Impl::O3CPU *)(this);
        assert(o3_tc->cpu);
        o3_tc->thread = this->thread[tid];

        // Setup quiesce event.
        this->thread[tid]->quiesceEvent = new EndQuiesceEvent(tc);

        // Give the thread the TC.
        this->thread[tid]->tc = tc;

        // Add the TC to the CPU's list of TC's.
        this->threadContexts.push_back(tc);
    }

    // FullO3CPU always requires an interrupt controller.
    if (!params->switched_out && interrupts.empty()) {
        fatal("FullO3CPU %s has no interrupt controller.\n"
              "Ensure createInterruptController() is called.\n", name());
    }

    for (ThreadID tid = 0; tid < this->numThreads; tid++)
        this->thread[tid]->setFuncExeInst(0);

    // Set up line tracer
    bool use_vmt_mode = (numThreads > 1) && (params->fetchWidth < numThreads);
    LineTracer* line_tracer = LineTracer::getLineTracer();
    line_tracer->addThreadContexts(numThreads, use_vmt_mode);
    line_tracer->setCpuClockPeriod(this->clockPeriod());
}

template <class Impl>
FullO3CPU<Impl>::~FullO3CPU()
{
}

template <class Impl>
void
FullO3CPU<Impl>::regProbePoints()
{
    BaseCPU::regProbePoints();

    ppInstAccessComplete = new ProbePointArg<PacketPtr>(getProbeManager(), "InstAccessComplete");
    ppDataAccessComplete = new ProbePointArg<std::pair<DynInstPtr, PacketPtr> >(getProbeManager(), "DataAccessComplete");

    fetch.regProbePoints();
    rename.regProbePoints();
    iew.regProbePoints();
    commit.regProbePoints();
}

template <class Impl>
void
FullO3CPU<Impl>::regStats()
{
    BaseO3CPU::regStats();

    // Register any of the O3CPU's stats here.
    timesIdled
        .name(name() + ".timesIdled")
        .desc("Number of times that the entire CPU went into an idle state and"
              " unscheduled itself")
        .prereq(timesIdled);

    idleCycles
        .name(name() + ".idleCycles")
        .desc("Total number of cycles that the CPU has spent unscheduled due "
              "to idling")
        .prereq(idleCycles);

    quiesceCycles
        .name(name() + ".quiesceCycles")
        .desc("Total number of cycles that CPU has spent quiesced or waiting "
              "for an interrupt")
        .prereq(quiesceCycles);

    // Number of Instructions simulated
    // --------------------------------
    // Should probably be in Base CPU but need templated
    // MaxThreads so put in here instead
    committedInsts
        .init(numThreads)
        .name(name() + ".committedInsts")
        .desc("Number of Instructions Simulated")
        .flags(Stats::total);

    committedOps
        .init(numThreads)
        .name(name() + ".committedOps")
        .desc("Number of Ops (including micro ops) Simulated")
        .flags(Stats::total);

    cpi
        .name(name() + ".cpi")
        .desc("CPI: Cycles Per Instruction")
        .precision(6);
    cpi = numCycles / committedInsts;

    totalCpi
        .name(name() + ".cpi_total")
        .desc("CPI: Total CPI of All Threads")
        .precision(6);
    totalCpi = numCycles / sum(committedInsts);

    ipc
        .name(name() + ".ipc")
        .desc("IPC: Instructions Per Cycle")
        .precision(6);
    ipc =  committedInsts / numCycles;

    totalIpc
        .name(name() + ".ipc_total")
        .desc("IPC: Total IPC of All Threads")
        .precision(6);
    totalIpc =  sum(committedInsts) / numCycles;

    this->fetch.regStats();
    this->decode.regStats();
    this->rename.regStats();
    this->iew.regStats();
    this->commit.regStats();
    this->rob.regStats();

    intRegfileReads
        .name(name() + ".int_regfile_reads")
        .desc("number of integer regfile reads")
        .prereq(intRegfileReads);

    intRegfileWrites
        .name(name() + ".int_regfile_writes")
        .desc("number of integer regfile writes")
        .prereq(intRegfileWrites);

    fpRegfileReads
        .name(name() + ".fp_regfile_reads")
        .desc("number of floating regfile reads")
        .prereq(fpRegfileReads);

    fpRegfileWrites
        .name(name() + ".fp_regfile_writes")
        .desc("number of floating regfile writes")
        .prereq(fpRegfileWrites);

    vecRegfileReads
        .name(name() + ".vec_regfile_reads")
        .desc("number of vector regfile reads")
        .prereq(vecRegfileReads);

    vecRegfileWrites
        .name(name() + ".vec_regfile_writes")
        .desc("number of vector regfile writes")
        .prereq(vecRegfileWrites);

    vecPredRegfileReads
        .name(name() + ".pred_regfile_reads")
        .desc("number of predicate regfile reads")
        .prereq(vecPredRegfileReads);

    vecPredRegfileWrites
        .name(name() + ".pred_regfile_writes")
        .desc("number of predicate regfile writes")
        .prereq(vecPredRegfileWrites);

    ccRegfileReads
        .name(name() + ".cc_regfile_reads")
        .desc("number of cc regfile reads")
        .prereq(ccRegfileReads);

    ccRegfileWrites
        .name(name() + ".cc_regfile_writes")
        .desc("number of cc regfile writes")
        .prereq(ccRegfileWrites);

    miscRegfileReads
        .name(name() + ".misc_regfile_reads")
        .desc("number of misc regfile reads")
        .prereq(miscRegfileReads);

    miscRegfileWrites
        .name(name() + ".misc_regfile_writes")
        .desc("number of misc regfile writes")
        .prereq(miscRegfileWrites);
}

template <class Impl>
void
FullO3CPU<Impl>::tick()
{
    // DPRINTF(O3CPU, "\n\nFullO3CPU: Ticking main, FullO3CPU.\n");
    assert(!switchedOut());
    assert(drainState() != DrainState::Drained);

    ++numCycles;
    updateCycleCounters(BaseCPU::CPU_STATE_ON);

//    activity = false;

    // Reset both icache and dcache ports. All cache ports are available in the
    // beginning of a cycle.
    icachePort.reset();
    dcachePort.reset();

    //Tick each of the stages
    fetch.tick();

    decode.tick();

    rename.tick();

    iew.tick();

    commit.tick();

    // Now advance the time buffers for all active threads
    for (auto tid : activeThreads) {
        timeBuffer[tid].advance();
        forwardTimeBuffer[tid].advance();
    }

    fetchQueue.advance();
    decodeQueue.advance();
    renameQueue.advance();
    iewQueue.advance();

    activityRec.advance();

    if (removeInstsThisCycle) {
        cleanUpRemovedInsts();
    }

    if (!tickEvent.scheduled()) {
        if (_status == SwitchedOut) {
            // DPRINTF(O3CPU, "Switched out!\n");
            // increment stat
            lastRunningCycle = curCycle();
        } else if (!activityRec.active() || _status == Idle) {
            // DPRINTF(O3CPU, "Idle!\n");
            lastRunningCycle = curCycle();
            timesIdled++;
        } else {
            schedule(tickEvent, clockEdge(Cycles(1)));
            // DPRINTF(O3CPU, "Scheduling next tick!\n");
        }
    }

    if (!FullSystem)
        updateThreadPriority();

    tryDrain();
}

template <class Impl>
void
FullO3CPU<Impl>::init()
{
    BaseCPU::init();

    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        // Set noSquashFromTC so that the CPU doesn't squash when initially
        // setting up registers.
        thread[tid]->noSquashFromTC = true;
        // Initialise the ThreadContext's memory proxies
        thread[tid]->initMemProxies(thread[tid]->getTC());
    }

    if (FullSystem && !params()->switched_out) {
        for (ThreadID tid = 0; tid < numThreads; tid++) {
            ThreadContext *src_tc = threadContexts[tid];
            TheISA::initCPU(src_tc, src_tc->contextId());
        }
    }

    // Clear noSquashFromTC.
    for (int tid = 0; tid < numThreads; ++tid)
        thread[tid]->noSquashFromTC = false;

    commit.setThreads(thread);
}

template <class Impl>
void
FullO3CPU<Impl>::startup()
{
    BaseCPU::startup();
    for (int tid = 0; tid < numThreads; ++tid)
        isa[tid]->startup(threadContexts[tid]);

    fetch.startupStage();
    decode.startupStage();
    iew.startupStage();
    rename.startupStage();
    commit.startupStage();
}

template <class Impl>
void
FullO3CPU<Impl>::activateThread(ThreadID tid)
{
    list<ThreadID>::iterator isActive =
        std::find(activeThreads.begin(), activeThreads.end(), tid);

    DPRINTF(O3CPU, "[tid:%p]: Calling activate thread.\n", this->thread[tid]);
    assert(!switchedOut());

    if (isActive == activeThreads.end()) {
        DPRINTF(O3CPU, "[tid:%i]: Adding to active threads list\n",
                tid);

        activeThreads.push_back(tid);
    }
}

template <class Impl>
void
FullO3CPU<Impl>::deactivateThread(ThreadID tid)
{
    //Remove From Active List, if Active
    list<ThreadID>::iterator thread_it =
        std::find(activeThreads.begin(), activeThreads.end(), tid);

    DPRINTF(O3CPU, "[tid:%i]: Calling deactivate thread.\n", tid);
    assert(!switchedOut());

    if (thread_it != activeThreads.end()) {
        DPRINTF(O3CPU,"[tid:%i]: Removing from active threads list\n",
                tid);
        activeThreads.erase(thread_it);
    }

    fetch.deactivateThread(tid);
    commit.deactivateThread(tid);
}

template <class Impl>
Counter
FullO3CPU<Impl>::totalInsts() const
{
    Counter total(0);

    ThreadID size = thread.size();
    for (ThreadID i = 0; i < size; i++)
        total += thread[i]->numInst;

    return total;
}

template <class Impl>
Counter
FullO3CPU<Impl>::totalOps() const
{
    Counter total(0);

    ThreadID size = thread.size();
    for (ThreadID i = 0; i < size; i++)
        total += thread[i]->numOp;

    return total;
}

template <class Impl>
void
FullO3CPU<Impl>::activateContext(ThreadID tid)
{
    assert(!switchedOut());

    // Needs to set each stage to running as well.
    activateThread(tid);

    // We don't want to wake the CPU if it is drained. In that case,
    // we just want to flag the thread as active and schedule the tick
    // event from drainResume() instead.
    if (drainState() == DrainState::Drained)
        return;

    // If we are time 0 or if the last activation time is in the past,
    // schedule the next tick and wake up the fetch unit
    if (lastActivatedCycle[tid] == 0 || lastActivatedCycle[tid] < curTick()) {
        scheduleTickEvent(Cycles(0));

        // Be sure to signal that there's some activity so the CPU doesn't
        // deschedule itself.
        activityRec.activity();

        // Signal fetch and commit stage that this thread is waking up
        fetch.wakeFromQuiesce(tid);
        commit.wakeFromQuiesce(tid);

        Cycles cycles(curCycle() - lastRunningCycle);
        // @todo: This is an oddity that is only here to match the stats
        if (cycles != 0)
            --cycles;
        quiesceCycles += cycles;

        lastActivatedCycle[tid] = curTick();

        _status = Running;

        BaseCPU::activateContext(tid);
    }
}

template <class Impl>
void
FullO3CPU<Impl>::suspendContext(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid: %i]: Suspending Thread Context.\n", tid);
    assert(!switchedOut());

    deactivateThread(tid);
    removeThread(tid);

    // If this was the last thread then unschedule the tick event.
    if (activeThreads.size() == 0) {
        unscheduleTickEvent();
        lastRunningCycle = curCycle();
        _status = Idle;
    }

    DPRINTF(Quiesce, "Suspending Context\n");

    BaseCPU::suspendContext(tid);
}

template <class Impl>
void
FullO3CPU<Impl>::haltContext(ThreadID tid)
{
    //For now, this is the same as deallocate
    DPRINTF(O3CPU,"[tid:%i]: Halt Context called. Deallocating\n", tid);
    assert(!switchedOut());

    deactivateThread(tid);
    removeThread(tid);

    updateCycleCounters(BaseCPU::CPU_STATE_SLEEP);
}

template <class Impl>
void
FullO3CPU<Impl>::insertThread(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Initializing thread into CPU");
    // Will change now that the PC and thread state is internal to the CPU
    // and not in the ThreadContext.
    ThreadContext *src_tc;
    if (FullSystem)
        src_tc = system->threadContexts[tid];
    else
        src_tc = tcBase(tid);

    //Bind Int Regs to Rename Map

    for (RegId reg_id(IntRegClass, 0); reg_id.index() < TheISA::NumIntRegs;
         reg_id.index()++) {
        PhysRegIdPtr phys_reg = freeList.getIntReg();
        renameMap[tid].setEntry(reg_id, phys_reg);
        scoreboard.setReg(phys_reg);
    }

    //Bind Float Regs to Rename Map
    for (RegId reg_id(FloatRegClass, 0); reg_id.index() < TheISA::NumFloatRegs;
         reg_id.index()++) {
        PhysRegIdPtr phys_reg = freeList.getFloatReg();
        renameMap[tid].setEntry(reg_id, phys_reg);
        scoreboard.setReg(phys_reg);
    }

    //Bind condition-code Regs to Rename Map
    for (RegId reg_id(CCRegClass, 0); reg_id.index() < TheISA::NumCCRegs;
         reg_id.index()++) {
        PhysRegIdPtr phys_reg = freeList.getCCReg();
        renameMap[tid].setEntry(reg_id, phys_reg);
        scoreboard.setReg(phys_reg);
    }

    //Copy Thread Data Into RegFile
    //this->copyFromTC(tid);

    //Set PC/NPC/NNPC
    pcState(src_tc->pcState(), tid);

    src_tc->setStatus(ThreadContext::Active);

    activateContext(tid);

    //Reset ROB/IQ/LSQ Entries
    commit.rob->resetEntries();
}

template <class Impl>
void
FullO3CPU<Impl>::removeThread(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Removing thread context from CPU.\n", tid);

    // clear all thread-specific states in each stage of the pipeline
    // since this thread is going to be completely removed from the CPU
    fetch.clearStates(tid);
    decode.clearStates(tid);
    rename.clearStates(tid);
    iew.clearStates(tid);
    commit.clearStates(tid);

    // drain all communication time buffers between stages. both forward and
    // backward buffers
    for (int i = 0; i < timeBuffer[tid].getSize(); ++i)
        timeBuffer[tid].advance();

    for (int i = 0; i < forwardTimeBuffer[tid].getSize(); ++i)
        forwardTimeBuffer[tid].advance();

    // @todo: 2-27-2008: Fix how we free up rename mappings
    // here to alleviate the case for double-freeing registers
    // in SMT workloads.

    // at this step, all instructions in the pipeline should be already
    // either committed successfully or squashed. All thread-specific
    // queues in the pipeline must be empty.
    assert(iew.instQueue.getCount(tid) == 0);
    assert(iew.ldstQueue.getCount(tid) == 0);
    assert(rob.isEmpty(tid));

    // Reset ROB/IQ/LSQ Entries

    // Commented out for now.  This should be possible to do by
    // telling all the pipeline stages to drain first, and then
    // checking until the drain completes.  Once the pipeline is
    // drained, call resetEntries(). - 10-09-06 ktlim
/*
    if (activeThreads.size() >= 1) {
        commit.rob->resetEntries();
        iew.resetEntries();
    }
*/
}

template <class Impl>
Fault
FullO3CPU<Impl>::hwrei(ThreadID tid)
{
#if THE_ISA == ALPHA_ISA
    // Need to clear the lock flag upon returning from an interrupt.
    this->setMiscRegNoEffect(AlphaISA::MISCREG_LOCKFLAG, false, tid);

    this->thread[tid]->kernelStats->hwrei();

    // FIXME: XXX check for interrupts? XXX
#endif
    return NoFault;
}

template <class Impl>
bool
FullO3CPU<Impl>::simPalCheck(int palFunc, ThreadID tid)
{
#if THE_ISA == ALPHA_ISA
    if (this->thread[tid]->kernelStats)
        this->thread[tid]->kernelStats->callpal(palFunc,
                                                this->threadContexts[tid]);

    switch (palFunc) {
      case PAL::halt:
        halt();
        if (--System::numSystemsRunning == 0)
            exitSimLoop("all cpus halted");
        break;

      case PAL::bpt:
      case PAL::bugchk:
        if (this->system->breakpoint())
            return false;
        break;
    }
#endif
    return true;
}

template <class Impl>
void
FullO3CPU<Impl>::switchRenameMode(ThreadID tid, UnifiedFreeList* freelist)
{
    auto pc = this->pcState(tid);

    // new_mode is the new vector renaming mode
    auto new_mode = RenameMode<TheISA::ISA>::mode(pc);

    // We update vecMode only if there has been a change
    if (new_mode != vecMode) {
        vecMode = new_mode;

        renameMap[tid].switchMode(vecMode);
        commitRenameMap[tid].switchMode(vecMode);
        renameMap[tid].switchFreeList(freelist);
    }
}

template <class Impl>
Fault
FullO3CPU<Impl>::getInterrupts()
{
    // Check if there are any outstanding interrupts
    return this->interrupts[0]->getInterrupt(this->threadContexts[0]);
}

template <class Impl>
void
FullO3CPU<Impl>::processInterrupts(const Fault &interrupt)
{
    // Check for interrupts here.  For now can copy the code that
    // exists within isa_fullsys_traits.hh.  Also assume that thread 0
    // is the one that handles the interrupts.
    // @todo: Possibly consolidate the interrupt checking code.
    // @todo: Allow other threads to handle interrupts.

    assert(interrupt != NoFault);
    this->interrupts[0]->updateIntrInfo(this->threadContexts[0]);

    // DPRINTF(O3CPU, "Interrupt %s being handled\n", interrupt->name());
    this->trap(interrupt, 0, nullptr);
}

template <class Impl>
void
FullO3CPU<Impl>::trap(const Fault &fault, ThreadID tid,
                      const StaticInstPtr &inst)
{
    // Pass the thread's TC into the invoke method.
    fault->invoke(this->threadContexts[tid], inst);
}

template <class Impl>
void
FullO3CPU<Impl>::syscall(int64_t callnum, ThreadID tid, Fault *fault)
{
    // DPRINTF(O3CPU, "[tid:%i] Executing syscall().\n\n", tid);

    DPRINTF(Activity,"Activity: syscall() called.\n");

    // Temporarily increase this by one to account for the syscall
    // instruction.
    ++(this->thread[tid]->funcExeInst);

    // Execute the actual syscall.
    this->thread[tid]->syscall(callnum, fault);

    // Decrease funcExeInst by one as the normal commit will handle
    // incrementing it.
    --(this->thread[tid]->funcExeInst);
}

template <class Impl>
void
FullO3CPU<Impl>::serializeThread(CheckpointOut &cp, ThreadID tid) const
{
    thread[tid]->serialize(cp);
}

template <class Impl>
void
FullO3CPU<Impl>::unserializeThread(CheckpointIn &cp, ThreadID tid)
{
    thread[tid]->unserialize(cp);
}

template <class Impl>
DrainState
FullO3CPU<Impl>::drain()
{
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    // If the CPU isn't doing anything, then return immediately.
    if (switchedOut())
        return DrainState::Drained;

    DPRINTF(Drain, "Draining...\n");

    // We only need to signal a drain to the commit stage as this
    // initiates squashing controls the draining. Once the commit
    // stage commits an instruction where it is safe to stop, it'll
    // squash the rest of the instructions in the pipeline and force
    // the fetch stage to stall. The pipeline will be drained once all
    // in-flight instructions have retired.
    commit.drain();

    // Wake the CPU and record activity so everything can drain out if
    // the CPU was not able to immediately drain.
    if (!isDrained())  {
        // If a thread is suspended, wake it up so it can be drained
        for (auto t : threadContexts) {
            if (t->status() == ThreadContext::Suspended){
                DPRINTF(Drain, "Currently suspended so activate %i \n",
                        t->threadId());
                t->activate();
                // As the thread is now active, change the power state as well
                activateContext(t->threadId());
            }
        }

        wakeCPU();
        activityRec.activity();

        DPRINTF(Drain, "CPU not drained\n");

        return DrainState::Draining;
    } else {
        DPRINTF(Drain, "CPU is already drained\n");
        if (tickEvent.scheduled())
            deschedule(tickEvent);

        // Flush out any old data from the time buffers.  In
        // particular, there might be some data in flight from the
        // fetch stage that isn't visible in any of the CPU buffers we
        // test in isDrained().
        for (ThreadID tid = 0; tid < numThreads; ++tid)
            for (int i = 0; i < timeBuffer[tid].getSize(); ++i)
                timeBuffer[tid].advance();

        for (int i = 0; i < fetchQueue.getSize(); ++i)
            fetchQueue.advance();

        for (int i = 0; i < decodeQueue.getSize(); ++i)
            decodeQueue.advance();

        for (int i = 0; i < renameQueue.getSize(); ++i)
            renameQueue.advance();

        for (int i = 0; i < iewQueue.getSize(); ++i)
            iewQueue.advance();

        drainSanityCheck();
        return DrainState::Drained;
    }
}

template <class Impl>
bool
FullO3CPU<Impl>::tryDrain()
{
    if (drainState() != DrainState::Draining || !isDrained())
        return false;

    if (tickEvent.scheduled())
        deschedule(tickEvent);

    DPRINTF(Drain, "CPU done draining, processing drain event\n");
    signalDrainDone();

    return true;
}

template <class Impl>
void
FullO3CPU<Impl>::drainSanityCheck() const
{
    assert(isDrained());
    fetch.drainSanityCheck();
    decode.drainSanityCheck();
    rename.drainSanityCheck();
    iew.drainSanityCheck();
    commit.drainSanityCheck();
}

template <class Impl>
bool
FullO3CPU<Impl>::isDrained() const
{
    bool drained(true);

    if (!instList.empty() || !removeList.empty()) {
        DPRINTF(Drain, "Main CPU structures not drained.\n");
        drained = false;
    }

    if (!fetch.isDrained()) {
        DPRINTF(Drain, "Fetch not drained.\n");
        drained = false;
    }

    if (!decode.isDrained()) {
        DPRINTF(Drain, "Decode not drained.\n");
        drained = false;
    }

    if (!rename.isDrained()) {
        DPRINTF(Drain, "Rename not drained.\n");
        drained = false;
    }

    if (!iew.isDrained()) {
        DPRINTF(Drain, "IEW not drained.\n");
        drained = false;
    }

    if (!commit.isDrained()) {
        DPRINTF(Drain, "Commit not drained.\n");
        drained = false;
    }

    return drained;
}

template <class Impl>
void
FullO3CPU<Impl>::commitDrained(ThreadID tid)
{
    fetch.drainStall(tid);
}

template <class Impl>
void
FullO3CPU<Impl>::drainResume()
{
    if (switchedOut())
        return;

    DPRINTF(Drain, "Resuming...num threads[%d]\n", thread.size());
    verifyMemoryMode();

    fetch.drainResume();
    commit.drainResume();

    _status = Idle;
    for (ThreadID i = 0; i < thread.size(); i++) {
        if (thread[i]->status() == ThreadContext::Active) {
            DPRINTF(Drain, "Activating thread: %i\n", i);
            activateThread(i);
            _status = Running;
        } else {
            DPRINTF(Drain, "Inactive thread[%i]\n", thread[i]->status()); 
        }
    }

    assert(!tickEvent.scheduled());
    if (_status == Running)
        schedule(tickEvent, nextCycle());

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();
}

template <class Impl>
void
FullO3CPU<Impl>::switchOut()
{
    DPRINTF(O3CPU, "Switching out\n");
    BaseCPU::switchOut();

    activityRec.reset();

    _status = SwitchedOut;

    if (checker)
        checker->switchOut();
}

template <class Impl>
void
FullO3CPU<Impl>::takeOverFrom(BaseCPU *oldCPU)
{
    BaseCPU::takeOverFrom(oldCPU);

    fetch.takeOverFrom();
    decode.takeOverFrom();
    rename.takeOverFrom();
    iew.takeOverFrom();
    commit.takeOverFrom();

    assert(!tickEvent.scheduled());

    FullO3CPU<Impl> *oldO3CPU = dynamic_cast<FullO3CPU<Impl>*>(oldCPU);
    if (oldO3CPU)
        globalSeqNum = oldO3CPU->globalSeqNum;

    lastRunningCycle = curCycle();
    _status = Idle;
}

template <class Impl>
void
FullO3CPU<Impl>::verifyMemoryMode() const
{
    if (!system->isTimingMode()) {
        fatal("The O3 CPU requires the memory system to be in "
              "'timing' mode.\n");
    }
}

template <class Impl>
RegVal
FullO3CPU<Impl>::readMiscRegNoEffect(int misc_reg, ThreadID tid) const
{
    return this->isa[tid]->readMiscRegNoEffect(misc_reg);
}

template <class Impl>
RegVal
FullO3CPU<Impl>::readMiscReg(int misc_reg, ThreadID tid)
{
    miscRegfileReads++;
    return this->isa[tid]->readMiscReg(misc_reg, tcBase(tid));
}

template <class Impl>
void
FullO3CPU<Impl>::setMiscRegNoEffect(int misc_reg, RegVal val, ThreadID tid)
{
    this->isa[tid]->setMiscRegNoEffect(misc_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setMiscReg(int misc_reg, RegVal val, ThreadID tid)
{
    miscRegfileWrites++;
    this->isa[tid]->setMiscReg(misc_reg, val, tcBase(tid));
}

template <class Impl>
RegVal
FullO3CPU<Impl>::readIntReg(PhysRegIdPtr phys_reg)
{
    intRegfileReads++;
    return regFile.readIntReg(phys_reg);
}

template <class Impl>
RegVal
FullO3CPU<Impl>::readFloatReg(PhysRegIdPtr phys_reg)
{
    fpRegfileReads++;
    return regFile.readFloatReg(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::readVecReg(PhysRegIdPtr phys_reg) const
        -> const VecRegContainer&
{
    vecRegfileReads++;
    return regFile.readVecReg(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::getWritableVecReg(PhysRegIdPtr phys_reg)
        -> VecRegContainer&
{
    vecRegfileWrites++;
    return regFile.getWritableVecReg(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::readVecElem(PhysRegIdPtr phys_reg) const -> const VecElem&
{
    vecRegfileReads++;
    return regFile.readVecElem(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::readVecPredReg(PhysRegIdPtr phys_reg) const
        -> const VecPredRegContainer&
{
    vecPredRegfileReads++;
    return regFile.readVecPredReg(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::getWritableVecPredReg(PhysRegIdPtr phys_reg)
        -> VecPredRegContainer&
{
    vecPredRegfileWrites++;
    return regFile.getWritableVecPredReg(phys_reg);
}

template <class Impl>
RegVal
FullO3CPU<Impl>::readCCReg(PhysRegIdPtr phys_reg)
{
    ccRegfileReads++;
    return regFile.readCCReg(phys_reg);
}

template <class Impl>
void
FullO3CPU<Impl>::setIntReg(PhysRegIdPtr phys_reg, RegVal val)
{
    intRegfileWrites++;
    regFile.setIntReg(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setFloatReg(PhysRegIdPtr phys_reg, RegVal val)
{
    fpRegfileWrites++;
    regFile.setFloatReg(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setVecReg(PhysRegIdPtr phys_reg, const VecRegContainer& val)
{
    vecRegfileWrites++;
    regFile.setVecReg(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setVecElem(PhysRegIdPtr phys_reg, const VecElem& val)
{
    vecRegfileWrites++;
    regFile.setVecElem(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setVecPredReg(PhysRegIdPtr phys_reg,
                               const VecPredRegContainer& val)
{
    vecPredRegfileWrites++;
    regFile.setVecPredReg(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setCCReg(PhysRegIdPtr phys_reg, RegVal val)
{
    ccRegfileWrites++;
    regFile.setCCReg(phys_reg, val);
}

template <class Impl>
RegVal
FullO3CPU<Impl>::readArchIntReg(int reg_idx, ThreadID tid)
{
    intRegfileReads++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
            RegId(IntRegClass, reg_idx));

    return regFile.readIntReg(phys_reg);
}

template <class Impl>
RegVal
FullO3CPU<Impl>::readArchFloatReg(int reg_idx, ThreadID tid)
{
    fpRegfileReads++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
        RegId(FloatRegClass, reg_idx));

    return regFile.readFloatReg(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::readArchVecReg(int reg_idx, ThreadID tid) const
        -> const VecRegContainer&
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecRegClass, reg_idx));
    return readVecReg(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::getWritableArchVecReg(int reg_idx, ThreadID tid)
        -> VecRegContainer&
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecRegClass, reg_idx));
    return getWritableVecReg(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::readArchVecElem(const RegIndex& reg_idx, const ElemIndex& ldx,
                                 ThreadID tid) const -> const VecElem&
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                                RegId(VecElemClass, reg_idx, ldx));
    return readVecElem(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::readArchVecPredReg(int reg_idx, ThreadID tid) const
        -> const VecPredRegContainer&
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecPredRegClass, reg_idx));
    return readVecPredReg(phys_reg);
}

template <class Impl>
auto
FullO3CPU<Impl>::getWritableArchVecPredReg(int reg_idx, ThreadID tid)
        -> VecPredRegContainer&
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecPredRegClass, reg_idx));
    return getWritableVecPredReg(phys_reg);
}

template <class Impl>
RegVal
FullO3CPU<Impl>::readArchCCReg(int reg_idx, ThreadID tid)
{
    ccRegfileReads++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
        RegId(CCRegClass, reg_idx));

    return regFile.readCCReg(phys_reg);
}

template <class Impl>
void
FullO3CPU<Impl>::setArchIntReg(int reg_idx, RegVal val, ThreadID tid)
{
    intRegfileWrites++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
            RegId(IntRegClass, reg_idx));

    regFile.setIntReg(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setArchFloatReg(int reg_idx, RegVal val, ThreadID tid)
{
    fpRegfileWrites++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
            RegId(FloatRegClass, reg_idx));

    regFile.setFloatReg(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setArchVecReg(int reg_idx, const VecRegContainer& val,
                               ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecRegClass, reg_idx));
    setVecReg(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setArchVecElem(const RegIndex& reg_idx, const ElemIndex& ldx,
                                const VecElem& val, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecElemClass, reg_idx, ldx));
    setVecElem(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setArchVecPredReg(int reg_idx, const VecPredRegContainer& val,
                                   ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
                RegId(VecPredRegClass, reg_idx));
    setVecPredReg(phys_reg, val);
}

template <class Impl>
void
FullO3CPU<Impl>::setArchCCReg(int reg_idx, RegVal val, ThreadID tid)
{
    ccRegfileWrites++;
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(
            RegId(CCRegClass, reg_idx));

    regFile.setCCReg(phys_reg, val);
}

template <class Impl>
TheISA::PCState
FullO3CPU<Impl>::pcState(ThreadID tid)
{
    return commit.pcState(tid);
}

template <class Impl>
void
FullO3CPU<Impl>::pcState(const TheISA::PCState &val, ThreadID tid)
{
    commit.pcState(val, tid);
}

template <class Impl>
Addr
FullO3CPU<Impl>::instAddr(ThreadID tid)
{
    return commit.instAddr(tid);
}

template <class Impl>
Addr
FullO3CPU<Impl>::nextInstAddr(ThreadID tid)
{
    return commit.nextInstAddr(tid);
}

template <class Impl>
MicroPC
FullO3CPU<Impl>::microPC(ThreadID tid)
{
    return commit.microPC(tid);
}

template <class Impl>
void
FullO3CPU<Impl>::squashFromTC(ThreadID tid)
{
    this->thread[tid]->noSquashFromTC = true;
    this->commit.generateTCEvent(tid);
}

template <class Impl>
typename FullO3CPU<Impl>::ListIt
FullO3CPU<Impl>::addInst(const DynInstPtr &inst)
{
    instList.push_back(inst);

    return --(instList.end());
}

template <class Impl>
void
FullO3CPU<Impl>::instDone(ThreadID tid, const DynInstPtr &inst)
{
    // Keep an instruction count.
    if (!inst->isMicroop() || inst->isLastMicroop()) {
        thread[tid]->numInst++;
        thread[tid]->numInsts++;
        committedInsts[tid]++;
        system->totalNumInsts++;

        // Check for instruction-count-based events.
        comInstEventQueue[tid]->serviceEvents(thread[tid]->numInst);
        system->instEventQueue.serviceEvents(system->totalNumInsts);
    }
    thread[tid]->numOp++;
    thread[tid]->numOps++;
    committedOps[tid]++;

    probeInstCommit(inst->staticInst);
}

template <class Impl>
void
FullO3CPU<Impl>::removeFrontInst(const DynInstPtr &inst)
{
    // DPRINTF(O3CPU, "Removing committed instruction [tid:%i] PC %s "
            // "[sn:%lli]\n",
            // inst->threadNumber, inst->pcState(), inst->seqNum);

    removeInstsThisCycle = true;

    // Remove the front instruction.
    removeList.push(inst->getInstListIt());
}

template <class Impl>
void
FullO3CPU<Impl>::removeInstsNotInROB(ThreadID tid)
{
    // DPRINTF(O3CPU, "Thread %i: Deleting instructions from instruction"
            // " list.\n", tid);

    ListIt end_it;

    bool rob_empty = false;

    if (instList.empty()) {
        return;
    } else if (rob.isEmpty(tid)) {
        // DPRINTF(O3CPU, "ROB is empty, squashing all insts.\n");
        end_it = instList.begin();
        rob_empty = true;
    } else {
        end_it = (rob.readTailInst(tid))->getInstListIt();
        // DPRINTF(O3CPU, "ROB is not empty, squashing insts not in ROB.\n");
    }

    removeInstsThisCycle = true;

    ListIt inst_it = instList.end();

    inst_it--;

    // Walk through the instruction list, removing any instructions
    // that were inserted after the given instruction iterator, end_it.
    while (inst_it != end_it) {
        assert(!instList.empty());

        squashInstIt(inst_it, tid);

        inst_it--;
    }

    // If the ROB was empty, then we actually need to remove the first
    // instruction as well.
    if (rob_empty) {
        squashInstIt(inst_it, tid);
    }
}

template <class Impl>
void
FullO3CPU<Impl>::removeInstsUntil(const InstSeqNum &seq_num, ThreadID tid)
{
    assert(!instList.empty());

    removeInstsThisCycle = true;

    ListIt inst_iter = instList.end();

    inst_iter--;

    // DPRINTF(O3CPU, "Deleting instructions from instruction "
            // "list that are from [tid:%i] and above [sn:%lli] (end=%lli).\n",
            // tid, seq_num, (*inst_iter)->seqNum);

    while ((*inst_iter)->seqNum > seq_num) {

        bool break_loop = (inst_iter == instList.begin());

        squashInstIt(inst_iter, tid);

        inst_iter--;

        if (break_loop)
            break;
    }
}

template <class Impl>
inline void
FullO3CPU<Impl>::squashInstIt(const ListIt &instIt, ThreadID tid)
{
    if ((*instIt)->threadNumber == tid) {
        // DPRINTF(O3CPU, "Squashing instruction, "
                // "[tid:%i] [sn:%lli] PC %s\n",
                // (*instIt)->threadNumber,
                // (*instIt)->seqNum,
                // (*instIt)->pcState());

        // Mark it as squashed.
        (*instIt)->setSquashed();

        // @todo: Formulate a consistent method for deleting
        // instructions from the instruction list
        // Remove the instruction from the list.
        removeList.push(instIt);
    }
}

template <class Impl>
void
FullO3CPU<Impl>::cleanUpRemovedInsts()
{
    while (!removeList.empty()) {
        // DPRINTF(O3CPU, "Removing instruction, "
                // "[tid:%i] [sn:%lli] PC %s\n",
                // (*removeList.front())->threadNumber,
                // (*removeList.front())->seqNum,
                // (*removeList.front())->pcState());

        instList.erase(removeList.front());

        removeList.pop();
    }

    removeInstsThisCycle = false;
}
/*
template <class Impl>
void
FullO3CPU<Impl>::removeAllInsts()
{
    instList.clear();
}
*/
template <class Impl>
void
FullO3CPU<Impl>::dumpInsts()
{
    int num = 0;

    ListIt inst_list_it = instList.begin();

    cprintf("Dumping Instruction List\n");

    while (inst_list_it != instList.end()) {
        cprintf("Instruction:%i\nPC:%#x\n[tid:%i]\n[sn:%lli]\nIssued:%i\n"
                "Squashed:%i\n\n",
                num, (*inst_list_it)->instAddr(), (*inst_list_it)->threadNumber,
                (*inst_list_it)->seqNum, (*inst_list_it)->isIssued(),
                (*inst_list_it)->isSquashed());
        inst_list_it++;
        ++num;
    }
}
/*
template <class Impl>
void
FullO3CPU<Impl>::wakeDependents(const DynInstPtr &inst)
{
    iew.wakeDependents(inst);
}
*/
template <class Impl>
void
FullO3CPU<Impl>::wakeCPU()
{
    if (activityRec.active() || tickEvent.scheduled()) {
        DPRINTF(Activity, "CPU already running.\n");
        return;
    }

    DPRINTF(Activity, "Waking up CPU\n");

    Cycles cycles(curCycle() - lastRunningCycle);
    // @todo: This is an oddity that is only here to match the stats
    if (cycles > 1) {
        --cycles;
        idleCycles += cycles;
        numCycles += cycles;
    }

    schedule(tickEvent, clockEdge());
}

template <class Impl>
void
FullO3CPU<Impl>::wakeup(ThreadID tid)
{
    if (this->thread[tid]->status() != ThreadContext::Suspended)
        return;

    this->wakeCPU();

    DPRINTF(Quiesce, "Suspended Processor woken\n");
    this->threadContexts[tid]->activate();
}

template <class Impl>
ThreadID
FullO3CPU<Impl>::getFreeTid()
{
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (!tids[tid]) {
            tids[tid] = true;
            return tid;
        }
    }

    return InvalidThreadID;
}

template <class Impl>
void
FullO3CPU<Impl>::updateThreadPriority()
{
    if (activeThreads.size() > 1) {
        //DEFAULT TO ROUND ROBIN SCHEME
        //e.g. Move highest priority to end of thread list
        list<ThreadID>::iterator list_begin = activeThreads.begin();

        unsigned high_thread = *list_begin;

        activeThreads.erase(list_begin);

        activeThreads.push_back(high_thread);
    }
}

template <class Impl>
bool
FullO3CPU<Impl>::isThreadHalting(ThreadID tid)
{
    return tcBase(tid)->status() == ThreadContext::Halting;
}

template <class Impl>
bool
FullO3CPU<Impl>::isThreadSuspending(ThreadID tid)
{
    return tcBase(tid)->status() == ThreadContext::Suspending;
}

template <class Impl>
void
FullO3CPU<Impl>::markThreadReadyToHalt(ThreadID tid)
{
    DPRINTF(O3CPU, "Marking [tid:%d] ready to halt\n", tid);
    assert(tcBase(tid)->status() == ThreadContext::Halting);
    assert(!haltingThreads[tid]);
    haltingThreads[tid] = true;

    // schedule an event in the next cycle to try to deactivate this thread
    if (!threadExitEvent.scheduled()) {
        schedule(threadExitEvent, nextCycle());
    }
}

template <class Impl>
void
FullO3CPU<Impl>::markThreadReadyToSuspend(ThreadID tid)
{
    DPRINTF(O3CPU, "Marking [tid:%d] ready to suspend\n", tid);
    assert(tcBase(tid)->status() == ThreadContext::Suspending);
    assert(!suspendingThreads[tid]);
    suspendingThreads[tid] = true;

    // schedule an event in the next cycle to try to deactivate this thread
    if (!threadExitEvent.scheduled()) {
        schedule(threadExitEvent, nextCycle());
    }
}

template <class Impl>
void
FullO3CPU<Impl>::markThreadToActivate(ThreadID tid)
{
    threadsToActivate[tid] = true;
}

template <class Impl>
void
FullO3CPU<Impl>::exitThreads()
{
    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        if (haltingThreads[tid] && rob.isEmpty(tid)) {
            haltContext(tid);
            tcBase(tid)->setStatus(ThreadContext::Halted);
            haltingThreads[tid] = false;

            if (threadsToActivate[tid]) {
                tcBase(tid)->activate();
                threadsToActivate[tid] = false;
            }
        } else if (suspendingThreads[tid] && rob.isEmpty(tid)) {
            suspendContext(tid);
            tcBase(tid)->setStatus(ThreadContext::Suspended);
            suspendingThreads[tid] = false;

            if (threadsToActivate[tid]) {
                tcBase(tid)->activate();
                threadsToActivate[tid] = false;
            }
        }
    }

    // check if any thread has not exited yet. If so, the thread should be
    // waiting for a squash to complete. We need to check back on this thread
    // in the next cycle.
    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        if (haltingThreads[tid] || suspendingThreads[tid]) {
            assert(!threadExitEvent.scheduled());
            schedule(threadExitEvent, nextCycle());
            return;
        }
    }
}

// Forward declaration of FullO3CPU.
template class FullO3CPU<O3CPUImpl>;
