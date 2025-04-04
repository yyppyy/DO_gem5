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
 * Copyright (c) 1999-2011 Mark D. Hill and David A. Wood
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

#include "mem/ruby/system/RubySystem.hh"

#include <fcntl.h>
#include <zlib.h>

#include <cstdio>
#include <list>

#include "base/intmath.hh"
#include "base/statistics.hh"
#include "debug/RubyCacheTrace.hh"
#include "debug/RubySystem.hh"
#include "mem/ruby/common/Address.hh"
#include "mem/ruby/common/MachineID.hh"
#include "mem/ruby/network/Network.hh"
#include "mem/ruby/structures/CacheMemory.hh"
#include "mem/simple_mem.hh"
#include "sim/eventq.hh"
#include "sim/simulate.hh"

using namespace std;

bool RubySystem::m_randomization;
uint32_t RubySystem::m_block_size_bytes;
uint32_t RubySystem::m_block_size_bits;
uint32_t RubySystem::m_memory_size_bits;
uint32_t RubySystem::m_l2_bits;
uint32_t RubySystem::m_real_phys_mem_bits;

bool RubySystem::m_warmup_enabled = false;
// To look forward to allowing multiple RubySystem instances, track the number
// of RubySystems that need to be warmed up on checkpoint restore.
unsigned RubySystem::m_systems_to_warmup = 0;
bool RubySystem::m_cooldown_enabled = false;

RubySystem::RubySystem(const Params *p)
    : ClockedObject(p), m_access_backing_store(p->access_backing_store),
      m_cache_recorder(NULL)
{
    m_randomization = p->randomization;

    m_block_size_bytes = p->block_size_bytes;
    assert(isPowerOf2(m_block_size_bytes));
    m_block_size_bits = floorLog2(m_block_size_bytes);
    m_memory_size_bits = p->memory_size_bits;
    m_l2_bits = p->l2_bits;
    m_real_phys_mem_bits = p->real_phys_mem_bits;

    // Resize to the size of different machine types
    m_abstract_controls.resize(MachineType_NUM);

    // Collate the statistics before they are printed.
    Stats::registerDumpCallback(new RubyStatsCallback(this));
    // Create the profiler
    m_profiler = new Profiler(p, this);
    m_phys_mem = p->phys_mem;
}

void
RubySystem::registerNetwork(Network* network_ptr)
{
    m_network = network_ptr;
}

void
RubySystem::registerAbstractController(AbstractController* cntrl)
{
    m_abs_cntrl_vec.push_back(cntrl);

    MachineID id = cntrl->getMachineID();
    m_abstract_controls[id.getType()][id.getNum()] = cntrl;
}

RubySystem::~RubySystem()
{
    delete m_network;
    delete m_profiler;
}

void
RubySystem::makeCacheRecorder(uint8_t *uncompressed_trace,
                              uint64_t cache_trace_size,
                              uint64_t block_size_bytes)
{
    vector<Sequencer*> sequencer_map;

// FIXME: @Tuan - comment out this temporarily for SC3 branch to work since
// SC3-AM has two different RubySystem. The RubySystem that is in charge of the
// active message network has no sequencer, so the following assertion will
// fail.
//
// TODO: @Tuan: probably the right thing to do with CacheRecorder class is to
// re-do it from scratch for the purpose of doing fast-forwarding.

//    Sequencer* sequencer_ptr = NULL;
//
//    for (int cntrl = 0; cntrl < m_abs_cntrl_vec.size(); cntrl++) {
//        sequencer_map.push_back(m_abs_cntrl_vec[cntrl]->getCPUSequencer());
//        if (sequencer_ptr == NULL) {
//            sequencer_ptr = sequencer_map[cntrl];
//        }
//    }
//
//    assert(sequencer_ptr != NULL);
//
//    for (int cntrl = 0; cntrl < m_abs_cntrl_vec.size(); cntrl++) {
//        if (sequencer_map[cntrl] == NULL) {
//            sequencer_map[cntrl] = sequencer_ptr;
//        }
//    }
//
    // Remove the old CacheRecorder if it's still hanging about.
    if (m_cache_recorder != NULL) {
        delete m_cache_recorder;
    }

    // Create the CacheRecorder and record the cache trace
    m_cache_recorder = new CacheRecorder(uncompressed_trace, cache_trace_size,
                                         sequencer_map, block_size_bytes);
}

void
RubySystem::memWriteback()
{
    // Make the trace so we know what to write back.
    DPRINTF(RubyCacheTrace, "Recording Cache Trace\n");
    makeCacheRecorder(NULL, 0, getBlockSizeBytes());
    for (int cntrl = 0; cntrl < m_abs_cntrl_vec.size(); cntrl++) {
        m_abs_cntrl_vec[cntrl]->recordCacheTrace(cntrl, m_cache_recorder);
    }
    DPRINTF(RubyCacheTrace, "Cache Trace Complete\n");

    // @Tuan: Loop through all records in the generated trace and write back
    // data if needed. This way assumes the interface between Ruby and memory
    // controllers is placed in directory controllers (MachineType_Directory).
    // For custom protocols, this assumption may not hold, so a wanring at the
    // end of this function is given.
    //
    // XXX: Future improvement: we should have a functional interface between
    // RubySystem and memory controller(s) for this kind of functional
    // operations instead of relying on specifc types of controllers.

    for (size_t i = 0; i < m_cache_recorder->getNumRecords(); ++i)
    {
        TraceRecord* rec_p = m_cache_recorder->getRecordAtIdx(i);
        assert(rec_p);
        assert(rec_p->m_write_mask_p);

        NodeID id = m_network->addressToNodeID(rec_p->m_data_address,
                                               MachineType_Directory);
        AbstractController* directory_p =
                        m_abstract_controls[MachineType_Directory][id];
        assert(directory_p);

        if (rec_p->m_type == RubyRequestType_LD ||
            rec_p->m_type == RubyRequestType_IFETCH) {
            // For records that are marked as RubyRequestType_LD, we assume
            // that the memory already has a valid copy of the data, so no
            // writeback is necessary. Therefore, no-op in this case.
        } else if (rec_p->m_type == RubyRequestType_ST &&
                   (rec_p->m_write_mask_p)->isFull()) {
            // For records that are marked as RubyRequestType_ST, the memory
            // controller may not have an up-to-date copy of the data yet, so
            // we need to write data back explicitly through functional
            // accesses
            auto req = std::make_shared<Request>(rec_p->m_data_address,
                                                 m_block_size_bytes,
                                                 0, Request::funcMasterId);
            MemCmd::Command cmd = MemCmd::WriteReq;
            Packet* pkt = new Packet(req, cmd);
            pkt->dataDynamic(new uint8_t[m_block_size_bytes]);
            pkt->setData(rec_p->m_data);

            directory_p->functionalMemoryWrite(pkt);

            delete pkt;
        } else if (rec_p->m_type == RubyRequestType_ST &&
                   !(rec_p->m_write_mask_p)->isEmpty()) {
            // Similar to the previous else-if case, but here we write back
            // bytes masked by the write mask
            for (size_t offset = 0; offset < getBlockSizeBytes(); ++offset) {
                if (!(rec_p->m_write_mask_p)->test(offset))
                    continue;

                auto req = std::make_shared<Request>
                                              (rec_p->m_data_address + offset,
                                               1, 0, Request::funcMasterId);
                MemCmd::Command cmd = MemCmd::WriteReq;
                Packet* pkt = new Packet(req, cmd);
                pkt->dataDynamic(new uint8_t);
                pkt->setData(rec_p->m_data);

                directory_p->functionalMemoryWrite(pkt);

                delete pkt;
            }
        } else {
            panic("RubySystem::memWriteback: Invalid request type\n");
        }
    }

    warn_once("Ruby memory writeback is experimental.  Continuing simulation "
              "afterwards may not always work as intended.");

    // Keep the cache recorder around so that we can dump the trace if a
    // checkpoint is immediately taken.
}

void
RubySystem::writeCompressedTrace(uint8_t *raw_data, string filename,
                                 uint64_t uncompressed_trace_size)
{
    // Create the checkpoint file for the memory
    string thefile = CheckpointIn::dir() + "/" + filename.c_str();

    int fd = creat(thefile.c_str(), 0664);
    if (fd < 0) {
        perror("creat");
        fatal("Can't open memory trace file '%s'\n", filename);
    }

    gzFile compressedMemory = gzdopen(fd, "wb");
    if (compressedMemory == NULL)
        fatal("Insufficient memory to allocate compression state for %s\n",
              filename);

    if (gzwrite(compressedMemory, raw_data, uncompressed_trace_size) !=
        uncompressed_trace_size) {
        fatal("Write failed on memory trace file '%s'\n", filename);
    }

    if (gzclose(compressedMemory)) {
        fatal("Close failed on memory trace file '%s'\n", filename);
    }
    delete[] raw_data;
}

void
RubySystem::serialize(CheckpointOut &cp) const
{
    // Store the cache-block size, so we are able to restore on systems with a
    // different cache-block size. CacheRecorder depends on the correct
    // cache-block size upon unserializing.
    uint64_t block_size_bytes = getBlockSizeBytes();
    SERIALIZE_SCALAR(block_size_bytes);

    // Check that there's a valid trace to use.  If not, then memory won't be
    // up-to-date and the simulation will probably fail when restoring from the
    // checkpoint.
    if (m_cache_recorder == NULL) {
        fatal("Call memWriteback() before serialize() to create ruby trace");
    }

    // Aggregate the trace entries together into a single array
    uint8_t *raw_data = new uint8_t[4096];
    uint64_t cache_trace_size = m_cache_recorder->aggregateRecords(&raw_data,
                                                                 4096);
    string cache_trace_file = name() + ".cache.gz";
    writeCompressedTrace(raw_data, cache_trace_file, cache_trace_size);

    SERIALIZE_SCALAR(cache_trace_file);
    SERIALIZE_SCALAR(cache_trace_size);
}

void
RubySystem::drainResume()
{
    // Delete the cache recorder if it was created in memWriteback()
    // to checkpoint the current cache state.
    if (m_cache_recorder) {
        delete m_cache_recorder;
        m_cache_recorder = NULL;
    }
}

void
RubySystem::readCompressedTrace(string filename, uint8_t *&raw_data,
                                uint64_t &uncompressed_trace_size)
{
    // Read the trace file
    gzFile compressedTrace;

    // trace file
    int fd = open(filename.c_str(), O_RDONLY);
    if (fd < 0) {
        perror("open");
        fatal("Unable to open trace file %s", filename);
    }

    compressedTrace = gzdopen(fd, "rb");
    if (compressedTrace == NULL) {
        fatal("Insufficient memory to allocate compression state for %s\n",
              filename);
    }

    raw_data = new uint8_t[uncompressed_trace_size];
    if (gzread(compressedTrace, raw_data, uncompressed_trace_size) <
            uncompressed_trace_size) {
        fatal("Unable to read complete trace from file %s\n", filename);
    }

    if (gzclose(compressedTrace)) {
        fatal("Failed to close cache trace file '%s'\n", filename);
    }
}

void
RubySystem::unserialize(CheckpointIn &cp)
{
    uint8_t *uncompressed_trace = NULL;

    // This value should be set to the checkpoint-system's block-size.
    // Optional, as checkpoints without it can be run if the
    // checkpoint-system's block-size == current block-size.
    uint64_t block_size_bytes = getBlockSizeBytes();
    UNSERIALIZE_OPT_SCALAR(block_size_bytes);

    string cache_trace_file;
    uint64_t cache_trace_size = 0;

    UNSERIALIZE_SCALAR(cache_trace_file);
    UNSERIALIZE_SCALAR(cache_trace_size);
    cache_trace_file = cp.cptDir + "/" + cache_trace_file;

    readCompressedTrace(cache_trace_file, uncompressed_trace,
                        cache_trace_size);
    m_warmup_enabled = true;
    m_systems_to_warmup++;

    // Create the cache recorder that will hang around until startup.
    makeCacheRecorder(uncompressed_trace, cache_trace_size, block_size_bytes);
}

void
RubySystem::startup()
{

    // Ruby restores state from a checkpoint by resetting the clock to 0 and
    // playing the requests that can possibly re-generate the cache state.
    // The clock value is set to the actual checkpointed value once all the
    // requests have been executed.
    //
    // This way of restoring state is pretty finicky. For example, if a
    // Ruby component reads time before the state has been restored, it would
    // cache this value and hence its clock would not be reset to 0, when
    // Ruby resets the global clock. This can potentially result in a
    // deadlock.
    //
    // The solution is that no Ruby component should read time before the
    // simulation starts. And then one also needs to hope that the time
    // Ruby finishes restoring the state is less than the time when the
    // state was checkpointed.

    if (m_warmup_enabled) {
        DPRINTF(RubyCacheTrace, "Starting ruby cache warmup\n");
        // save the current tick value
        Tick curtick_original = curTick();
        // save the event queue head
        Event* eventq_head = eventq->replaceHead(NULL);
        // set curTick to 0 and reset Ruby System's clock
        setCurTick(0);
        resetClock();

        // Schedule an event to start cache warmup
        enqueueRubyEvent(curTick());
        simulate();

        delete m_cache_recorder;
        m_cache_recorder = NULL;
        m_systems_to_warmup--;
        if (m_systems_to_warmup == 0) {
            m_warmup_enabled = false;
        }

        // Restore eventq head
        eventq->replaceHead(eventq_head);
        // Restore curTick and Ruby System's clock
        setCurTick(curtick_original);
        resetClock();
    }

    resetStats();
}

void
RubySystem::processRubyEvent()
{
    if (getWarmupEnabled()) {
        m_cache_recorder->enqueueNextFetchRequest();
    } else if (getCooldownEnabled()) {
        m_cache_recorder->enqueueNextFlushRequest();
    }
}

void
RubySystem::resetStats()
{
    m_start_cycle = curCycle();
}

bool
RubySystem::functionalRead(PacketPtr pkt)
{
    Addr address(pkt->getAddr());
    Addr line_address = makeLineAddress(address);

    AccessPermission access_perm = AccessPermission_NotPresent;
    int num_controllers = m_abs_cntrl_vec.size();

    DPRINTF(RubySystem, "Functional Read request for %#x\n", address);

    unsigned int num_ro = 0;
    unsigned int num_rw = 0;
    unsigned int num_busy = 0;
    unsigned int num_backing_store = 0;
    unsigned int num_invalid = 0;

    // In this loop we count the number of controllers that have the given
    // address in read only, read write and busy states.
    for (unsigned int i = 0; i < num_controllers; ++i) {
        access_perm = m_abs_cntrl_vec[i]-> getAccessPermission(line_address);
        if (access_perm == AccessPermission_Read_Only)
            num_ro++;
        else if (access_perm == AccessPermission_Read_Write)
            num_rw++;
        else if (access_perm == AccessPermission_Busy)
            num_busy++;
        else if (access_perm == AccessPermission_Backing_Store)
            // See RubySlicc_Exports.sm for details, but Backing_Store is meant
            // to represent blocks in memory *for Broadcast/Snooping protocols*,
            // where memory has no idea whether it has an exclusive copy of data
            // or not.
            num_backing_store++;
        else if (access_perm == AccessPermission_Invalid ||
                 access_perm == AccessPermission_NotPresent)
            num_invalid++;
    }

    assert(num_rw <= 1);

    DPRINTF(RubySystem, "num_busy = %d, num_ro = %d, num_rw = %d, "
            "num_backing_store = %d, num_invalid = %d\n",
            num_busy, num_ro, num_rw, num_backing_store, num_invalid);

    // This if case is meant to capture what happens in a Broadcast/Snoop
    // protocol where the block does not exist in the cache hierarchy. You
    // only want to read from the Backing_Store memory if there is no copy in
    // the cache hierarchy, otherwise you want to try to read the RO or RW
    // copies existing in the cache hierarchy (covered by the else statement).
    // The reason is because the Backing_Store memory could easily be stale, if
    // there are copies floating around the cache hierarchy, so you want to read
    // it only if it's not in the cache hierarchy at all.

    if (num_invalid == (num_controllers - 1) && num_backing_store == 1) {
        DPRINTF(RubySystem, "only copy in Backing_Store memory, "
                "read from it\n");
        for (unsigned int i = 0; i < num_controllers; ++i) {
            access_perm = m_abs_cntrl_vec[i]->getAccessPermission(line_address);
            if (access_perm == AccessPermission_Backing_Store) {
                m_abs_cntrl_vec[i]->functionalRead(line_address, pkt);
                return true;
            }
        }
    } else if (num_ro > 0 || num_rw == 1) {
        // In Broadcast/Snoop protocols, this covers if you know the block
        // exists somewhere in the caching hierarchy, then you want to read any
        // valid RO or RW block.  In directory protocols, same thing, you want
        // to read any valid readable copy of the block.
        DPRINTF(RubySystem, "num_busy = %d, num_ro = %d, num_rw = %d\n",
                num_busy, num_ro, num_rw);
        // In this loop, we try to figure which controller has a read only or
        // a read write copy of the given address. Any valid copy would suffice
        // for a functional read.
        for (unsigned int i = 0;i < num_controllers;++i) {
            access_perm = m_abs_cntrl_vec[i]->getAccessPermission(line_address);
            if (access_perm == AccessPermission_Read_Only ||
                access_perm == AccessPermission_Read_Write) {
                m_abs_cntrl_vec[i]->functionalRead(line_address, pkt);
                return true;
            }
        }
    } else {
        // None of the controller has read access of the data. This may
        // be because of all controllers are in transient states so that
        // their access permissions are busy. We check if the data is in
        // the network
        DPRINTF(RubySystem, "num_busy = %d, num_ro = %d, num_rw = %d\n",
                num_busy, num_ro, num_rw);
        DPRINTF(RubySystem, "read from the network.\n");
        if (m_network->functionalRead(pkt))
          return true;

        // The network does not have the data either. This could be the
        // case that the controller that previously owned the data has
        // received a data request and has forward the data to the
        // requestor (e.g. a cache controller received a Fwd_GETX request
        // and has sent the data and has transitioned to I
        // state). However, the message containing the data has not been
        // put in the network yet. In this case we simply search all
        // controllers that may still hold a copy of the data although
        // they no longer own the data.
        DPRINTF(RubySystem, "Network does not have the data either\n");

        for (unsigned int i = 0;i < num_controllers;++i) {
            if (m_abs_cntrl_vec[i]->functionalRead(line_address, pkt))
                return true;
        }
    }

    return false;
}

// The function searches through all the buffers that exist in different
// cache, directory and memory controllers, and in the network components
// and writes the data portion of those that hold the address specified
// in the packet.
bool
RubySystem::functionalWrite(PacketPtr pkt)
{
    Addr addr(pkt->getAddr());
    Addr line_addr = makeLineAddress(addr);
    AccessPermission access_perm = AccessPermission_NotPresent;
    int num_controllers = m_abs_cntrl_vec.size();

    DPRINTF(RubySystem, "Functional Write request for %#x\n", addr);

    uint32_t M5_VAR_USED num_functional_writes = 0;

    for (unsigned int i = 0; i < num_controllers;++i) {
        num_functional_writes +=
            m_abs_cntrl_vec[i]->functionalWriteBuffers(pkt);

        access_perm = m_abs_cntrl_vec[i]->getAccessPermission(line_addr);
        if (access_perm != AccessPermission_Invalid &&
            access_perm != AccessPermission_NotPresent) {
            num_functional_writes +=
                m_abs_cntrl_vec[i]->functionalWrite(line_addr, pkt);
        }
    }

    num_functional_writes += m_network->functionalWrite(pkt);
    DPRINTF(RubySystem, "Messages written = %u\n", num_functional_writes);

    return true;
}

#ifdef CHECK_COHERENCE
// This code will check for cases if the given cache block is exclusive in
// one node and shared in another-- a coherence violation
//
// To use, the SLICC specification must call sequencer.checkCoherence(address)
// when the controller changes to a state with new permissions.  Do this
// in setState.  The SLICC spec must also define methods "isBlockShared"
// and "isBlockExclusive" that are specific to that protocol
//
void
RubySystem::checkGlobalCoherenceInvariant(const Address& addr)
{
#if 0
    NodeID exclusive = -1;
    bool sharedDetected = false;
    NodeID lastShared = -1;

    for (int i = 0; i < m_chip_vector.size(); i++) {
        if (m_chip_vector[i]->isBlockExclusive(addr)) {
            if (exclusive != -1) {
                // coherence violation
                WARN_EXPR(exclusive);
                WARN_EXPR(m_chip_vector[i]->getID());
                WARN_EXPR(addr);
                WARN_EXPR(getTime());
                ERROR_MSG("Coherence Violation Detected -- 2 exclusive chips");
            } else if (sharedDetected) {
                WARN_EXPR(lastShared);
                WARN_EXPR(m_chip_vector[i]->getID());
                WARN_EXPR(addr);
                WARN_EXPR(getTime());
                ERROR_MSG("Coherence Violation Detected -- exclusive chip with >=1 shared");
            } else {
                exclusive = m_chip_vector[i]->getID();
            }
        } else if (m_chip_vector[i]->isBlockShared(addr)) {
            sharedDetected = true;
            lastShared = m_chip_vector[i]->getID();

            if (exclusive != -1) {
                WARN_EXPR(lastShared);
                WARN_EXPR(exclusive);
                WARN_EXPR(addr);
                WARN_EXPR(getTime());
                ERROR_MSG("Coherence Violation Detected -- exclusive chip with >=1 shared");
            }
        }
    }
#endif
}
#endif

RubySystem *
RubySystemParams::create()
{
    return new RubySystem(this);
}
