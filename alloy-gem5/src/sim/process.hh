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

/*
 * Copyright (c) 2014-2016 Advanced Micro Devices, Inc.
 * Copyright (c) 2001-2005 The Regents of The University of Michigan
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
 * Authors: Nathan Binkert
 *          Steve Reinhardt
 *          Brandon Potter
 */

#ifndef __PROCESS_HH__
#define __PROCESS_HH__

#include <inttypes.h>

#include <map>
#include <string>
#include <vector>
#include <utility>
#include <cassert>

#include "arch/registers.hh"
#include "base/statistics.hh"
#include "base/types.hh"
#include "config/the_isa.hh"
#include "mem/se_translating_port_proxy.hh"
#include "sim/fd_array.hh"
#include "sim/fd_entry.hh"
#include "sim/mem_state.hh"
#include "sim/sim_object.hh"
#include "sim/system.hh"
#include "mem/page_table.hh"
#include "debug/RRC.hh"
#include "debug/DOMEM.hh"

struct ProcessParams;

class EmulatedDriver;
class ObjectFile;
class EmulationPageTable;
class SyscallDesc;
class SyscallReturn;
class System;
class ThreadContext;

class Process : public SimObject
{
  public:
    Process(ProcessParams *params, EmulationPageTable *pTable,
            ObjectFile *obj_file);

    void serialize(CheckpointOut &cp) const override;
    void unserialize(CheckpointIn &cp) override;

    void initState() override;
    DrainState drain() override;

    virtual void syscall(int64_t callnum, ThreadContext *tc, Fault *fault);
    virtual RegVal getSyscallArg(ThreadContext *tc, int &i) = 0;
    virtual RegVal getSyscallArg(ThreadContext *tc, int &i, int width);
    virtual void setSyscallArg(ThreadContext *tc, int i, RegVal val) = 0;
    virtual void setSyscallReturn(ThreadContext *tc,
                                  SyscallReturn return_value) = 0;
    virtual SyscallDesc *getDesc(int callnum) = 0;

    inline uint64_t uid() { return _uid; }
    inline uint64_t euid() { return _euid; }
    inline uint64_t gid() { return _gid; }
    inline uint64_t egid() { return _egid; }
    inline uint64_t pid() { return _pid; }
    inline uint64_t ppid() { return _ppid; }
    inline uint64_t pgid() { return _pgid; }
    inline uint64_t tgid() { return _tgid; }
    inline void setpgid(uint64_t pgid) { _pgid = pgid; }

    const char *progName() const { return executable.c_str(); }
    std::string fullPath(const std::string &filename);
    std::string getcwd() const { return cwd; }

    /**
     * Find an emulated device driver.
     *
     * @param filename Name of the device (under /dev)
     * @return Pointer to driver object if found, else nullptr
     */
    EmulatedDriver *findDriver(std::string filename);

    // This function acts as a callback to update the bias value in
    // the object file because the parameters needed to calculate the
    // bias are not available when the object file is created.
    void updateBias();
    Addr getBias();
    Addr getStartPC();
    ObjectFile *getInterpreter();

    // override of virtual SimObject method: register statistics
    void regStats() override;

    void allocateMem(Addr vaddr, int64_t size, bool clobber = false);
    void allocateMemThreadCtx(Addr vaddr, int64_t size, bool clobber = false, ThreadContext *tc = nullptr);

    void barrierInit(uint64_t n_threads) {
      system->n_barrier_waiters = n_threads;
      system->n_barrier_in = 0;
      system->n_barrier_out = 0;
    }

    void barrierWait(uint64_t bar_id) {
      ++(system->n_barrier_in);
      assert(system->n_barrier_in <= system->n_barrier_waiters);
    }
  
    int barrierReady(int cpu_id) {
      if (system->n_barrier_in == system->n_barrier_waiters) {
        ++(system->n_barrier_out);
        if (system->n_barrier_out == system->n_barrier_waiters) {
          system->n_barrier_in = 0;
          system->n_barrier_out = 0;          
        }
        return 1;
      } else {
        return 0;
      }
    }

    void registerSTNTBeginVaddr(Addr vaddr_begin, int cpu_id) {
      stnt_begin_vaddr[cpu_id] = vaddr_begin;
      // DPRINTF(DOMEM, "register stnt begin vaddr[0x%0llx]\n", vaddr_begin);
    }
  
    void registerSTNTEndVaddr(Addr vaddr_end, int cpu_id) {
      system->STNTVaddrRanges.emplace_back(std::make_pair(stnt_begin_vaddr[cpu_id], vaddr_end));
      // DPRINTF(DOMEM, "register stnt range vaddr[0x%0llx-0x%llx]\n", stnt_begin_vaddr, vaddr_end);
    }
  
    void registerSTRELBeginVaddr(Addr vaddr_begin, int cpu_id) {
      strel_begin_vaddr[cpu_id] = vaddr_begin;      
      // DPRINTF(DOMEM, "register strel begin vaddr[0x%0llx]\n", vaddr_begin);
    }
  
    void registerSTRELEndVaddr(Addr vaddr_end, int cpu_id) {
      system->STRELVaddrRanges.emplace_back(std::make_pair(strel_begin_vaddr[cpu_id], vaddr_end));   
      // DPRINTF(DOMEM, "register strel range vaddr[0x%0llx-0x%llx]\n", strel_begin_vaddr, vaddr_end);   
    }

    bool isSTNTVaddrRange(Addr paddr) {
      Addr begin_paddr, end_paddr;
      for (auto range : system->STNTVaddrRanges) {
        Addr begin_vaddr = range.first;
        Addr end_vaddr = (range.second - 1); // -1 is a must
        if (!pTable->translate(begin_vaddr, begin_paddr)) {
          // panic("stnt quirying unmapped vaddr[0x%x]\n", begin_vaddr);
          return false;
        } else {
          // begin_paddr += (begin_vaddr % TheISA::PageBytes);
          if (!pTable->translate(end_vaddr, end_paddr)) {
            // panic("stnt quirying unmapped vaddr[0x%x]\n", end_vaddr);
            return false;
          } else {
            // end_paddr += (end_vaddr % TheISA::PageBytes);
            // DPRINTF(DOMEM, "stnt check paddr[0x%llx] for range v[0x%llx-0x%llx], p[0x%llx-0x%llx]\n",
              // paddr, begin_vaddr, end_vaddr, begin_paddr, end_paddr);
            if (paddr >= begin_paddr && paddr < end_paddr) {
              return true;
            }
          }
        }
      }
      return false;
    }

    bool isSTRELVaddrRange(Addr paddr) {
      Addr begin_paddr, end_paddr;
      for (auto range : system->STRELVaddrRanges) {
        Addr begin_vaddr = range.first;
        Addr end_vaddr = (range.second - 1); // -1 is a must
        if (!pTable->translate(begin_vaddr, begin_paddr)) {
          // panic("strel quirying unmapped vaddr[0x%x]\n", begin_vaddr);
          return false;
        } else {
          // begin_paddr += (begin_vaddr % TheISA::PageBytes);
          if (!pTable->translate(end_vaddr, end_paddr)) {
            // panic("strel quirying unmapped vaddr[0x%x]\n", end_vaddr);
            return false;
          } else {
            // end_paddr += (end_vaddr % TheISA::PageBytes);
            // DPRINTF(DOMEM, "strel check paddr[0x%llx] for range v[0x%llx-0x%llx], p[0x%llx-0x%llx]\n",
              // paddr, begin_vaddr, end_vaddr, begin_paddr, end_paddr);
            if (paddr >= begin_paddr && paddr < end_paddr) {
              return true;
            }
          }
        }
      }
      return false;
    }

    /// Attempt to fix up a fault at vaddr by allocating a page on the stack.
    /// @return Whether the fault has been fixed.
    bool fixupStackFault(Addr vaddr);
    bool fixupStackFaultThreadCtx(Addr vaddr, ThreadContext *tc);

    // After getting registered with system object, tell process which
    // system-wide context id it is assigned.
    void
    assignThreadContext(ContextID context_id)
    {
        contextIds.push_back(context_id);
    }

    // Find a free context to use
    ThreadContext *findFreeContext();

    /**
     * After delegating a thread context to a child process
     * no longer should relate to the ThreadContext
     */
    void revokeThreadContext(int context_id);

    /**
     * Does mmap region grow upward or downward from mmapEnd?  Most
     * platforms grow downward, but a few (such as Alpha) grow upward
     * instead, so they can override this method to return false.
     */
    virtual bool mmapGrowsDown() const { return true; }

    /**
     * Maps a contiguous range of virtual addresses in this process's
     * address space to a contiguous range of physical addresses.
     * This function exists primarily to expose the map operation to
     * python, so that configuration scripts can set up mappings in SE mode.
     *
     * @param vaddr The starting virtual address of the range.
     * @param paddr The starting physical address of the range.
     * @param size The length of the range in bytes.
     * @param cacheable Specifies whether accesses are cacheable.
     * @return True if the map operation was successful.  (At this
     *           point in time, the map operation always succeeds.)
     */
    bool map(Addr vaddr, Addr paddr, int size, bool cacheable = true);

    void replicatePage(Addr vaddr, Addr new_paddr, ThreadContext *old_tc,
                       ThreadContext *new_tc, bool alloc_page);

    virtual void clone(ThreadContext *old_tc, ThreadContext *new_tc,
                       Process *new_p, RegVal flags);

    // thread contexts associated with this process
    std::vector<ContextID> contextIds;

    // system object which owns this process
    System *system;

    Stats::Scalar numSyscalls;  // track how many system calls are executed

    bool useArchPT; // flag for using architecture specific page table
    bool kvmInSE;   // running KVM requires special initialization

    EmulationPageTable *pTable;

    SETranslatingPortProxy initVirtMem; // memory proxy for initial image load

    std::map<int, Addr> stnt_begin_vaddr;
    std::map<int, Addr> strel_begin_vaddr;

    ObjectFile *objFile;
    std::vector<std::string> argv;
    std::vector<std::string> envp;
    std::string cwd;
    std::string executable;

    // Id of the owner of the process
    uint64_t _uid;
    uint64_t _euid;
    uint64_t _gid;
    uint64_t _egid;

    // pid of the process and it's parent
    uint64_t _pid;
    uint64_t _ppid;
    uint64_t _pgid;
    uint64_t _tgid;

    // Emulated drivers available to this process
    std::vector<EmulatedDriver *> drivers;

    std::shared_ptr<FDArray> fds;

    bool *exitGroup;
    std::shared_ptr<MemState> memState;

    /**
     * Calls a futex wakeup at the address specified by this pointer when
     * this process exits.
     */
    uint64_t childClearTID;

    // Process was forked with SIGCHLD set.
    bool *sigchld;

    /**
     * True if this process needs to init args passed from an object file
     * (e.g., the very first process created when the loader loads an object
     * file into gem5).
     *
     * False if this process does not need to init args. An example of this
     * would be processes created by clone system call and sharing virtual
     * address space with their parent.
     */
    bool needToInitArgs;
};

#endif // __PROCESS_HH__