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
 * Copyright (c) 2012-2013,2015 ARM Limited
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

#ifndef __CPU_SIMPLE_TIMING_HH__
#define __CPU_SIMPLE_TIMING_HH__

#include "arch/registers.hh"
#include "cpu/simple/base.hh"
#include "cpu/simple/exec_context.hh"
#include "cpu/translation.hh"
#include "params/TimingSimpleCPU.hh"

class TimingSimpleCPU : public BaseSimpleCPU
{
  public:

    TimingSimpleCPU(TimingSimpleCPUParams * params);
    virtual ~TimingSimpleCPU();

    void init() override;

  private:

    /*
     * If an access needs to be broken into fragments, currently at most two,
     * the the following two classes are used as the sender state of the
     * packets so the CPU can keep track of everything. In the main packet
     * sender state, there's an array with a spot for each fragment. If a
     * fragment has already been accepted by the CPU, aka isn't waiting for
     * a retry, it's pointer is NULL. After each fragment has successfully
     * been processed, the "outstanding" counter is decremented. Once the
     * count is zero, the entire larger access is complete.
     */
    class SplitMainSenderState : public Packet::SenderState
    {
      public:
        int outstanding;
        PacketPtr fragments[2];

        int
        getPendingFragment()
        {
            if (fragments[0]) {
                return 0;
            } else if (fragments[1]) {
                return 1;
            } else {
                return -1;
            }
        }
    };

    class SplitFragmentSenderState : public Packet::SenderState
    {
      public:
        SplitFragmentSenderState(PacketPtr _bigPkt, int _index) :
            bigPkt(_bigPkt), index(_index)
        {}
        PacketPtr bigPkt;
        int index;

        void
        clearFromParent()
        {
            SplitMainSenderState * main_send_state =
                dynamic_cast<SplitMainSenderState *>(bigPkt->senderState);
            main_send_state->fragments[index] = NULL;
        }
    };

    class FetchTranslation : public BaseTLB::Translation
    {
      protected:
        TimingSimpleCPU *cpu;

      public:
        FetchTranslation(TimingSimpleCPU *_cpu)
            : cpu(_cpu)
        {}

        void
        markDelayed()
        {
            assert(cpu->_status == BaseSimpleCPU::Running);
            cpu->_status = ITBWaitResponse;
        }

        void
        finish(const Fault &fault, const RequestPtr &req, ThreadContext *tc,
               BaseTLB::Mode mode)
        {
            cpu->sendFetch(fault, req, tc);
        }
    };
    FetchTranslation fetchTranslation;

    void threadSnoop(PacketPtr pkt, ThreadID sender);
    void sendData(const RequestPtr &req,
                  uint8_t *data, uint64_t *res, bool read);
    void sendSplitData(const RequestPtr &req1, const RequestPtr &req2,
                       const RequestPtr &req,
                       uint8_t *data, bool read);

    void translationFault(const Fault &fault);

    PacketPtr buildPacket(const RequestPtr &req, bool read);
    void buildSplitPacket(PacketPtr &pkt1, PacketPtr &pkt2,
            const RequestPtr &req1, const RequestPtr &req2,
            const RequestPtr &req,
            uint8_t *data, bool read);

    bool handleReadPacket(PacketPtr pkt);
    // This function always implicitly uses dcache_pkt.
    bool handleWritePacket();

    /**
     * A TimingCPUPort overrides the default behaviour of the
     * recvTiming and recvRetry and implements events for the
     * scheduling of handling of incoming packets in the following
     * cycle.
     */
    class TimingCPUPort : public MasterPort
    {
      public:

        TimingCPUPort(const std::string& _name, TimingSimpleCPU* _cpu)
            : MasterPort(_name, _cpu), cpu(_cpu),
              retryRespEvent([this]{ sendRetryResp(); }, name())
        { }

      protected:

        TimingSimpleCPU* cpu;

        struct TickEvent : public Event
        {
            PacketPtr pkt;
            TimingSimpleCPU *cpu;

            TickEvent(TimingSimpleCPU *_cpu) : pkt(NULL), cpu(_cpu) {}
            const char *description() const { return "Timing CPU tick"; }
            void schedule(PacketPtr _pkt, Tick t);
        };

        EventFunctionWrapper retryRespEvent;
    };

    class IcachePort : public TimingCPUPort
    {
      public:

        IcachePort(TimingSimpleCPU *_cpu)
            : TimingCPUPort(_cpu->name() + ".icache_port", _cpu),
              tickEvent(_cpu)
        { }

      protected:

        virtual bool recvTimingResp(PacketPtr pkt);

        virtual void recvReqRetry();

        struct ITickEvent : public TickEvent
        {

            ITickEvent(TimingSimpleCPU *_cpu)
                : TickEvent(_cpu) {}
            void process();
            const char *description() const { return "Timing CPU icache tick"; }
        };

        ITickEvent tickEvent;

    };

    class DcachePort : public TimingCPUPort
    {
      public:

        DcachePort(TimingSimpleCPU *_cpu)
            : TimingCPUPort(_cpu->name() + ".dcache_port", _cpu),
              tickEvent(_cpu)
        {
           cacheBlockMask = ~(cpu->cacheLineSize() - 1);
        }

        Addr cacheBlockMask;
      protected:

        /** Snoop a coherence request, we need to check if this causes
         * a wakeup event on a cpu that is monitoring an address
         */
        virtual void recvTimingSnoopReq(PacketPtr pkt);
        virtual void recvFunctionalSnoop(PacketPtr pkt);

        virtual bool recvTimingResp(PacketPtr pkt);

        virtual void recvReqRetry();

        virtual bool isSnooping() const {
            return true;
        }

        struct DTickEvent : public TickEvent
        {
            DTickEvent(TimingSimpleCPU *_cpu)
                : TickEvent(_cpu) {}
            void process();
            const char *description() const { return "Timing CPU dcache tick"; }
        };

        DTickEvent tickEvent;

    };

    //-------------------------------------------------------------------------
    // OutReqPort
    //-------------------------------------------------------------------------
    // Port that delivers requests from and responses to this test harness

    class OutReqPort : public TimingCPUPort
    {
      public:
        OutReqPort(TimingSimpleCPU* _cpu, const std::string& _name)
            : TimingCPUPort(_cpu->name() + _name, _cpu),
              tickEvent(_cpu)
        { }

        ~OutReqPort() = default;

      protected:
        bool recvTimingResp(Packet *pkt) override;

        void recvReqRetry() override
        { panic("recvReqRetry not implemented\n"); }

        void recvTimingSnoopReq(Packet *pkt) override
        { panic("recvTimingSnoopReq Not implemented!\n"); }

        Tick recvAtomicSnoop(Packet *pkt) override
        { panic("recvAtomicSnoop Not implemented!\n"); }

        struct AMTickEvent : public TickEvent
        {
            AMTickEvent(TimingSimpleCPU *_cpu)
                : TickEvent(_cpu) {}
            void process();
            const char *description() const { return "Timing CPU OutReqPort tick"; }
        };

        AMTickEvent tickEvent;
    };

    //-------------------------------------------------------------------------
    // InReqPort
    //-------------------------------------------------------------------------
    // Port that delivers requests from and responses to the outside test
    // object

    class InReqPort : public SlavePort
    {
      public:
        InReqPort(TimingSimpleCPU* _cpu, const std::string& _name)
            : SlavePort(_name, _cpu), pendingPkt(nullptr), cpu(_cpu)
        { }

        ~InReqPort() = default;

        PacketPtr pendingPkt;

      protected:
        bool recvTimingReq(Packet *pkt) override;

        void recvFunctional(Packet *pkt) override
        { panic("recvFunctional Not implemented\n"); }
        Tick recvAtomic(Packet *pkt) override
        { panic("recvAtomic Not implemented\n"); }

        void recvRespRetry() override;

        AddrRangeList getAddrRanges() const override
        { panic("getAddrRanges Not implemented!\n"); }

      private:
        TimingSimpleCPU* cpu;
    };

    void updateCycleCounts();

    IcachePort icachePort;
    DcachePort dcachePort;

    OutReqPort outReqPort;
    InReqPort  inReqPort;

    PacketPtr ifetch_pkt;
    PacketPtr dcache_pkt;

    Cycles previousCycle;
    Counter previousInsts;

  protected:

     /** Return a reference to the data port. */
    MasterPort &getDataPort() override { return dcachePort; }

    /** Return a reference to the instruction port. */
    MasterPort &getInstPort() override { return icachePort; }

    MasterPort &getOutNetworkReqPort() override { return outReqPort; }

    SlavePort &getInNetworkReqPort() override { return inReqPort; }

  public:

    DrainState drain() override;
    void drainResume() override;

    void switchOut() override;
    void takeOverFrom(BaseCPU *oldCPU) override;

    void verifyMemoryMode() const override;

    void activateContext(ThreadID thread_num) override;
    void suspendContext(ThreadID thread_num) override;

    Fault initiateMemRead(Addr addr, unsigned size,
                          Request::Flags flags) override;

    Fault writeMem(uint8_t *data, unsigned size,
                   Addr addr, Request::Flags flags, uint64_t *res) override;

    Fault initiateMemAMO(Addr addr, unsigned size, Request::Flags flags,
                         AtomicOpFunctor *amo_op) override;

    void handleMemFence();

    void fetch();
    void sendFetch(const Fault &fault,
                   const RequestPtr &req, ThreadContext *tc);
    void completeIfetch(PacketPtr );
    void completeDataAccess(PacketPtr pkt);
    void advanceInst(const Fault &fault);

    /** This function is used by the page table walker to determine if it could
     * translate the a pending request or if the underlying request has been
     * squashed. This always returns false for the simple timing CPU as it never
     * executes any instructions speculatively.
     * @ return Is the current instruction squashed?
     */
    bool isSquashed() const { return false; }

    /**
     * Print state of address in memory system via PrintReq (for
     * debugging).
     */
    void printAddr(Addr a);

    /**
     * Finish a DTB translation.
     * @param state The DTB translation state.
     */
    void finishTranslation(WholeTranslationState *state);

    void regStats() override;

  private:

    EventFunctionWrapper fetchEvent;

    struct IprEvent : Event {
        Packet *pkt;
        TimingSimpleCPU *cpu;
        IprEvent(Packet *_pkt, TimingSimpleCPU *_cpu, Tick t);
        virtual void process();
        virtual const char *description() const;
    };

    /**
     * Check if a system is in a drained state.
     *
     * We need to drain if:
     * <ul>
     * <li>We are in the middle of a microcode sequence as some CPUs
     *     (e.g., HW accelerated CPUs) can't be started in the middle
     *     of a gem5 microcode sequence.
     *
     * <li>Stay at PC is true.
     *
     * <li>A fetch event is scheduled. Normally this would never be the
     *     case with microPC() == 0, but right after a context is
     *     activated it can happen.
     * </ul>
     */
    bool isDrained() {
        SimpleExecContext& t_info = *threadInfo[curThread];
        SimpleThread* thread = t_info.thread;

        return thread->microPC() == 0 && !t_info.stayAtPC &&
               !fetchEvent.scheduled();
    }

    /**
     * Try to complete a drain request.
     *
     * @returns true if the CPU is drained, false otherwise.
     */
    bool tryCompleteDrain();

#if THE_ISA == RISCV_ISA
    // moyang TC3-ULI
    void sendULIReq() override;
    void sendULIResp() override;
    void sendULIRespNack() override;
#endif

    // stats
#if THE_ISA == RISCV_ISA
  public:
    Cycles flushStartCycle;
    Cycles applRuntimeStartCycle;
    Cycles applEnqueueStartCycle;
    Cycles applDequeueStartCycle;
    Cycles applStealStartCycle;

    // appl runtime stats

    Stats::Scalar appl_runtime_cycles;
    Stats::Scalar appl_num_enqueue;
    Stats::Scalar appl_enqueue_cycles;
    Stats::Scalar appl_num_dequeue;
    Stats::Scalar appl_dequeue_cycles;
    Stats::Scalar appl_num_steal_successful;
    Stats::Scalar appl_steal_successful_cycles;
    Stats::Scalar appl_num_steal_failed;
    Stats::Scalar appl_steal_failed_cycles;
    Stats::Scalar appl_num_execute;
    Stats::Scalar appl_num_self_invalidation;
    Stats::Scalar appl_num_self_writeback;

    // active messages

    Cycles active_message_start_cycle;

    Stats::Scalar num_active_message_ack;
    Stats::Scalar active_message_ack_latency;
    Stats::Scalar num_active_message_nack;
    Stats::Scalar active_message_nack_latency;

    // CPU stats

    Cycles dcache_start_cycle;
    Cycles icache_start_cycle;

    Stats::Scalar in_task_cycles;
    Stats::Scalar in_task_insts;
    Stats::Scalar in_task_icache_cycles;
    Stats::Scalar in_task_dcache_cycles;
    Stats::Scalar in_task_dcache_load_cycles;
    Stats::Scalar in_task_dcache_load_hit_cycles;
    Stats::Scalar in_task_dcache_load_miss_cycles;
    Stats::Scalar in_task_dcache_store_cycles;
    Stats::Scalar in_task_dcache_store_hit_cycles;
    Stats::Scalar in_task_dcache_store_miss_cycles;
    Stats::Scalar in_task_dcache_amo_cycles;
    Stats::Scalar in_task_dcache_ll_cycles;
    Stats::Scalar in_task_dcache_sc_cycles;
    Stats::Scalar in_task_dcache_flush_cycles;

    Stats::Scalar in_task_icache_num_ifetch;
    Stats::Scalar in_task_dcache_num_access;
    Stats::Scalar in_task_dcache_num_load;
    Stats::Scalar in_task_dcache_num_load_hit;
    Stats::Scalar in_task_dcache_num_load_miss;
    Stats::Scalar in_task_dcache_num_store;
    Stats::Scalar in_task_dcache_num_store_hit;
    Stats::Scalar in_task_dcache_num_store_miss;
    Stats::Scalar in_task_dcache_num_amo;
    Stats::Scalar in_task_dcache_num_ll;
    Stats::Scalar in_task_dcache_num_sc;
    Stats::Scalar in_task_dcache_num_flush;

    Stats::Scalar in_runtime_cycles;
    Stats::Scalar in_runtime_insts;
    Stats::Scalar in_runtime_icache_cycles;
    Stats::Scalar in_runtime_dcache_cycles;
    Stats::Scalar in_runtime_dcache_load_cycles;
    Stats::Scalar in_runtime_dcache_load_hit_cycles;
    Stats::Scalar in_runtime_dcache_load_miss_cycles;
    Stats::Scalar in_runtime_dcache_store_cycles;
    Stats::Scalar in_runtime_dcache_store_hit_cycles;
    Stats::Scalar in_runtime_dcache_store_miss_cycles;
    Stats::Scalar in_runtime_dcache_amo_cycles;
    Stats::Scalar in_runtime_dcache_ll_cycles;
    Stats::Scalar in_runtime_dcache_sc_cycles;
    Stats::Scalar in_runtime_dcache_flush_cycles;

    Stats::Scalar in_runtime_icache_num_ifetch;
    Stats::Scalar in_runtime_dcache_num_access;
    Stats::Scalar in_runtime_dcache_num_load;
    Stats::Scalar in_runtime_dcache_num_load_hit;
    Stats::Scalar in_runtime_dcache_num_load_miss;
    Stats::Scalar in_runtime_dcache_num_store;
    Stats::Scalar in_runtime_dcache_num_store_hit;
    Stats::Scalar in_runtime_dcache_num_store_miss;
    Stats::Scalar in_runtime_dcache_num_amo;
    Stats::Scalar in_runtime_dcache_num_ll;
    Stats::Scalar in_runtime_dcache_num_sc;
    Stats::Scalar in_runtime_dcache_num_flush;

    bool in_runtime;

    void toggle_stats_en(bool on) override;
    void toggleCSR(int misc_reg, RegVal reg_val) override;
#endif
};

#endif // __CPU_SIMPLE_TIMING_HH__
