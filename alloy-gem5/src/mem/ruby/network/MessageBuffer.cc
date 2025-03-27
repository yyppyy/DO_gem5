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

#include "mem/ruby/network/MessageBuffer.hh"

#include <cassert>

#include "base/cprintf.hh"
#include "base/logging.hh"
#include "base/random.hh"
#include "base/stl_helpers.hh"
#include "debug/RubyQueue.hh"
#include "debug/DOSTORAGE.hh"
#include "mem/protocol/MessageSizeType.hh"
#include "mem/protocol/RequestMsg.hh"
#include "mem/protocol/ResponseMsg.hh"
#include "mem/ruby/network/Network.hh"
#include "mem/ruby/system/RubySystem.hh"

using namespace std;
using m5::stl_helpers::operator<<;

Stats::Vector MessageBuffer::m_coh_msg_count;
Stats::Vector MessageBuffer::m_coh_msg_bytes;

bool MessageBuffer::m_init_static_stats = false;

MessageBuffer::MessageBuffer(const Params *p)
    : SimObject(p), m_stall_map_size(0),
    m_max_size(p->buffer_size), m_time_last_time_size_checked(0),
    m_time_last_time_enqueue(0), m_time_last_time_pop(0),
    m_last_arrival_time(0), m_strict_fifo(p->ordered),
    m_randomization(p->randomization)
{
    m_msg_counter = 0;
    m_consumer = NULL;
    m_size_last_time_size_checked = 0;
    m_size_at_cycle_start = 0;
    m_msgs_this_cycle = 0;
    m_priority_rank = 0;

    m_stall_msg_map.clear();
    m_input_link_id = 0;
    m_vnet_id = 0;

    m_buf_msgs = 0;
    m_stall_time = 0;

    m_dequeue_callback = nullptr;
}

unsigned int
MessageBuffer::getSize(Tick curTime)
{
    if (m_time_last_time_size_checked != curTime) {
        m_time_last_time_size_checked = curTime;
        m_size_last_time_size_checked = m_prio_heap.size();
    }

    return m_size_last_time_size_checked;
}

bool
MessageBuffer::areNSlotsAvailable(unsigned int n, Tick current_time)
{

    // fast path when message buffers have infinite size
    if (m_max_size == 0) {
        return true;
    }

    // determine the correct size for the current cycle
    // pop operations shouldn't effect the network's visible size
    // until schd cycle, but enqueue operations effect the visible
    // size immediately
    unsigned int current_size = 0;

    if (m_time_last_time_pop < current_time) {
        // no pops this cycle - heap size is correct
        current_size = m_prio_heap.size();
    } else {
        if (m_time_last_time_enqueue < current_time) {
            // no enqueues this cycle - m_size_at_cycle_start is correct
            current_size = m_size_at_cycle_start;
        } else {
            // both pops and enqueues occured this cycle - add new
            // enqueued msgs to m_size_at_cycle_start
            current_size = m_size_at_cycle_start + m_msgs_this_cycle;
        }
    }

    // now compare the new size with our max size
    if (current_size + m_stall_map_size + n <= m_max_size) {
        return true;
    } else {
        DPRINTF(RubyQueue, "n: %d, current_size: %d, heap size: %d, "
                "m_max_size: %d\n",
                n, current_size, m_prio_heap.size(), m_max_size);
        m_not_avail_count++;
        return false;
    }
}

const Message*
MessageBuffer::peek() const
{
    DPRINTF(RubyQueue, "Peeking at head of queue.\n");
    const Message* msg_ptr = m_prio_heap.front().get();
    assert(msg_ptr);

    // DPRINTF(RubyQueue, "Message: %s\n", (*msg_ptr));
    return msg_ptr;
}

// FIXME - move me somewhere else
Tick
random_time()
{
    Tick time = 1;
    time += random_mt.random(0, 3);  // [0...3]
    if (random_mt.random(0, 7) == 0) {  // 1 in 8 chance
        time += 100 + random_mt.random(1, 15); // 100 + [1...15]
    }
    return time;
}

void
MessageBuffer::delayAllMessages(Tick current_time, Tick delta)
{   
    DPRINTF(RubyQueue, "Delaying all messages. current[%lu] heap front[%lu]\n", current_time, m_prio_heap.front()->getLastEnqueueTime());
    assert(isReady(current_time));
    Tick shift = current_time - m_prio_heap.front()->getLastEnqueueTime();
    for (MsgPtr message : m_prio_heap) {
        Message* msg_ptr = message.get();
        Tick future_time = msg_ptr->getLastEnqueueTime() + shift + delta;
        // assert(msg_ptr->getLastEnqueueTime() >= current_time);
        msg_ptr->setLastEnqueueTime(future_time);
        m_consumer->scheduleEventAbsolute(future_time);
    }
}

void
MessageBuffer::enqueue(MsgPtr message, Tick current_time, Tick delta)
{
    // record current time incase we have a pop that also adjusts my size
    if (m_time_last_time_enqueue < current_time) {
        m_msgs_this_cycle = 0;  // first msg this cycle
        m_time_last_time_enqueue = current_time;
    }

    m_msg_counter++;
    m_msgs_this_cycle++;

    // Calculate the arrival time of the message, that is, the first
    // cycle the message can be dequeued.
    assert(delta > 0);
    Tick arrival_time = 0;

    // random delays are inserted if either RubySystem level randomization flag
    // is turned on, or the buffer level randomization is set
    if (!RubySystem::getRandomization() && !m_randomization) {
        // No randomization
        arrival_time = current_time + delta;
    } else {
        // Randomization - ignore delta
        if (m_strict_fifo) {
            if (m_last_arrival_time < current_time) {
                m_last_arrival_time = current_time;
            }
            arrival_time = m_last_arrival_time + random_time();
        } else {
            arrival_time = current_time + random_time();
        }
    }

    // Check the arrival time
    assert(arrival_time > current_time);
    if (m_strict_fifo) {
        if (arrival_time < m_last_arrival_time) {
            panic("FIFO ordering violated: %s name: %s current time: %d "
                  "delta: %d arrival_time: %d last arrival_time: %d\n",
                  *this, name(), current_time, delta, arrival_time,
                  m_last_arrival_time);
        }
    }

    // If running a cache trace, don't worry about the last arrival checks
    if (!RubySystem::getWarmupEnabled()) {
        m_last_arrival_time = arrival_time;
    }

    // compute the delay cycles and set enqueue time
    Message* msg_ptr = message.get();
    assert(msg_ptr != NULL);

    assert(current_time >= msg_ptr->getLastEnqueueTime() &&
           "ensure we aren't dequeued early");

    msg_ptr->updateDelayedTicks(current_time);
    msg_ptr->setLastEnqueueTime(arrival_time);
    msg_ptr->setMsgCounter(m_msg_counter);

    // Insert the message into the priority heap
    m_prio_heap.push_back(message);
    push_heap(m_prio_heap.begin(), m_prio_heap.end(), greater<MsgPtr>());
    // Increment the number of messages statistic
    m_buf_msgs++;

    // DPRINTF(RubyQueue, "Enqueue arrival_time: %lld, Message: %s\n",
            // arrival_time, *(message.get()));
    DPRINTF(RubyQueue, "Enqueue arrival_time: %lld\n", arrival_time);

    // Schedule the wakeup
    assert(m_consumer != NULL);
    m_consumer->scheduleEventAbsolute(arrival_time);
    m_consumer->storeEventInfo(m_vnet_id);
}

Tick
MessageBuffer::dequeue(Tick current_time, bool decrement_messages)
{
    DPRINTF(RubyQueue, "Popping\n");
    assert(isReady(current_time));

    DPRINTF(RubyQueue, "Current queue size: %ld\n", m_prio_heap.size());

    // get MsgPtr of the message about to be dequeued
    MsgPtr message = m_prio_heap.front();
    // const Message* msg_ptr = message.get();
    // DPRINTF(RubyQueue, "Message: %s\n", (*msg_ptr));

    // get the delay cycles
    message->updateDelayedTicks(current_time);
    Tick delay = message->getDelayedTicks();

    m_stall_time = curTick() - message->getTime();

    // record previous size and time so the current buffer size isn't
    // adjusted until schd cycle
    if (m_time_last_time_pop < current_time) {
        m_size_at_cycle_start = m_prio_heap.size();
        m_time_last_time_pop = current_time;
    }

    pop_heap(m_prio_heap.begin(), m_prio_heap.end(), greater<MsgPtr>());
    m_prio_heap.pop_back();
    if (decrement_messages) {
        // If the message will be removed from the queue, decrement the
        // number of message in the queue.
        m_buf_msgs--;
    }

    // if a dequeue callback was requested, call it now
    if (m_dequeue_callback) {
        m_dequeue_callback();
    }

    // @tuan: profile coherence requests/responses only if this dequeue is not
    // a stall dequeue (i.e., decrement_messsages is true), the consumer of
    // this message is a controller (e.g., cache and directory controllers)
    if (decrement_messages &&
        m_consumer &&
        dynamic_cast<AbstractController*>(m_consumer)) {

        // try cast the message to RequestMsg
        RequestMsg* req_msg_ptr = dynamic_cast<RequestMsg*>(message.get());
        if (req_msg_ptr) {
          unsigned int i = req_msg_ptr->getType();
          m_coh_msg_count[i]++;
          m_coh_msg_bytes[i] +=
               Network::MessageSizeType_to_int(req_msg_ptr->getMessageSize());
        }

        // try cast the message to ResponseMsg
        ResponseMsg* resp_msg_ptr = dynamic_cast<ResponseMsg*>(message.get());
        if (resp_msg_ptr) {
          unsigned int i = CoherenceRequestType_NUM + resp_msg_ptr->getType();
          m_coh_msg_count[i]++;
          m_coh_msg_bytes[i] +=
               Network::MessageSizeType_to_int(resp_msg_ptr->getMessageSize());
        }
    }

    recycled_st_rels.erase(message.get());

    return delay;
}

void
MessageBuffer::registerDequeueCallback(std::function<void()> callback)
{
    m_dequeue_callback = callback;
}

void
MessageBuffer::unregisterDequeueCallback()
{
    m_dequeue_callback = nullptr;
}

void
MessageBuffer::clear()
{
    m_prio_heap.clear();

    m_msg_counter = 0;
    m_time_last_time_enqueue = 0;
    m_time_last_time_pop = 0;
    m_size_at_cycle_start = 0;
    m_msgs_this_cycle = 0;
}

void
MessageBuffer::recycle(Tick current_time, Tick recycle_latency)
{
    DPRINTF(RubyQueue, "Recycling.\n");
    assert(isReady(current_time));
    MsgPtr node = m_prio_heap.front();
    pop_heap(m_prio_heap.begin(), m_prio_heap.end(), greater<MsgPtr>());

    Tick future_time = current_time + recycle_latency;
    node->setLastEnqueueTime(future_time);

    m_prio_heap.back() = node;
    push_heap(m_prio_heap.begin(), m_prio_heap.end(), greater<MsgPtr>());
    m_consumer->scheduleEventAbsolute(future_time);

    recycled_st_rels.insert(node.get());
    DPRINTF(DOSTORAGE, "recycledStRel %lu\n", recycled_st_rels.size());
}

void
MessageBuffer::reanalyzeList(list<MsgPtr> &lt, Tick schdTick)
{
    while (!lt.empty()) {
        MsgPtr m = lt.front();
        assert(m->getLastEnqueueTime() <= schdTick);

        m_prio_heap.push_back(m);
        push_heap(m_prio_heap.begin(), m_prio_heap.end(),
                  greater<MsgPtr>());

        m_consumer->scheduleEventAbsolute(schdTick);

        DPRINTF(RubyQueue, "Requeue arrival_time: %lld, Message: %s\n",
            schdTick, *(m.get()));

        lt.pop_front();
    }
}

void
MessageBuffer::reanalyzeMessages(Addr addr, Tick current_time)
{
    DPRINTF(RubyQueue, "ReanalyzeMessages %#x\n", addr);
    assert(m_stall_msg_map.count(addr) > 0);

    //
    // Put all stalled messages associated with this address back on the
    // prio heap.  The reanalyzeList call will make sure the consumer is
    // scheduled for the current cycle so that the previously stalled messages
    // will be observed before any younger messages that may arrive this cycle
    //
    m_stall_map_size -= m_stall_msg_map[addr].size();
    assert(m_stall_map_size >= 0);
    reanalyzeList(m_stall_msg_map[addr], current_time);
    m_stall_msg_map.erase(addr);
}

void
MessageBuffer::reanalyzeAllMessages(Tick current_time)
{
    DPRINTF(RubyQueue, "ReanalyzeAllMessages\n");

    //
    // Put all stalled messages associated with this address back on the
    // prio heap.  The reanalyzeList call will make sure the consumer is
    // scheduled for the current cycle so that the previously stalled messages
    // will be observed before any younger messages that may arrive this cycle.
    //
    for (StallMsgMapType::iterator map_iter = m_stall_msg_map.begin();
         map_iter != m_stall_msg_map.end(); ++map_iter) {
        m_stall_map_size -= map_iter->second.size();
        assert(m_stall_map_size >= 0);
        reanalyzeList(map_iter->second, current_time);
    }
    m_stall_msg_map.clear();
}

void
MessageBuffer::stallMessage(Addr addr, Tick current_time)
{
    DPRINTF(RubyQueue, "Stalling due to %#x\n", addr);
    assert(isReady(current_time));
    assert(getOffset(addr) == 0);
    MsgPtr message = m_prio_heap.front();

    // Since the message will just be moved to stall map, indicate that the
    // buffer should not decrement the m_buf_msgs statistic
    dequeue(current_time, false);

    //
    // Note: no event is scheduled to analyze the map at a later time.
    // Instead the controller is responsible to call reanalyzeMessages when
    // these addresses change state.
    //
    (m_stall_msg_map[addr]).push_back(message);
    m_stall_map_size++;
    m_stall_count++;
}

void
MessageBuffer::print(ostream& out) const
{
    ccprintf(out, "[MessageBuffer: ");
    if (m_consumer != NULL) {
        ccprintf(out, " consumer-yes ");
    }

    vector<MsgPtr> copy(m_prio_heap);
    sort_heap(copy.begin(), copy.end(), greater<MsgPtr>());
    ccprintf(out, "%s] %s", copy, name());
}

bool
MessageBuffer::isReady(Tick current_time) const
{
    // DPRINTF(RubyQueue, "size:%d, cur time:%d, front time:%d\n",
    //     m_prio_heap.size(),
    //     current_time,
    //     (m_prio_heap.size() > 0) ? m_prio_heap.front()->getLastEnqueueTime() : 0);
    return ((m_prio_heap.size() > 0) &&
        (m_prio_heap.front()->getLastEnqueueTime() <= current_time));
}

void
MessageBuffer::regStats()
{
    m_not_avail_count
        .name(name() + ".not_avail_count")
        .desc("Number of times this buffer did not have N slots available")
        .flags(Stats::nozero);

    m_buf_msgs
        .name(name() + ".avg_buf_msgs")
        .desc("Average number of messages in buffer")
        .flags(Stats::nozero);

    m_stall_count
        .name(name() + ".num_msg_stalls")
        .desc("Number of times messages were stalled")
        .flags(Stats::nozero);

    m_occupancy
        .name(name() + ".avg_buf_occ")
        .desc("Average occupancy of buffer capacity")
        .flags(Stats::nozero);

    m_stall_time
        .name(name() + ".avg_stall_time")
        .desc("Average number of cycles messages are stalled in this MB")
        .flags(Stats::nozero);

    if (m_max_size > 0) {
        m_occupancy = m_buf_msgs / m_max_size;
    } else {
        m_occupancy = 0;
    }

    if (!m_init_static_stats) {
        m_coh_msg_count
            .init(CoherenceRequestType_NUM + CoherenceResponseType_NUM)
            .name("coh_msg_count")
            .desc("number of coherence msgs arriving in cache controllers")
            .flags(Stats::nozero);

        m_coh_msg_bytes
            .init(CoherenceRequestType_NUM + CoherenceResponseType_NUM)
            .name("coh_msg_bytes")
            .desc("size of coherence msgs arriving in cache controllers")
            .flags(Stats::nozero);

        for (CoherenceRequestType type = CoherenceRequestType_FIRST;
             type < CoherenceRequestType_NUM; ++type) {
            m_coh_msg_count.subname((unsigned int) type,
                                    CoherenceRequestType_to_string(type));
            m_coh_msg_bytes.subname((unsigned int) type,
                                    CoherenceRequestType_to_string(type));
        }

        for (CoherenceResponseType type = CoherenceResponseType_FIRST;
             type < CoherenceResponseType_NUM; ++type) {
            unsigned int i = (unsigned int) CoherenceRequestType_NUM +
                             (unsigned int) type;
            m_coh_msg_count.subname(i, CoherenceResponseType_to_string(type));
            m_coh_msg_bytes.subname(i, CoherenceResponseType_to_string(type));
        }

        m_init_static_stats = true;
    }
}

uint32_t
MessageBuffer::functionalWrite(Packet *pkt)
{
    uint32_t num_functional_writes = 0;

    // Check the priority heap and write any messages that may
    // correspond to the address in the packet.
    for (unsigned int i = 0; i < m_prio_heap.size(); ++i) {
        Message *msg = m_prio_heap[i].get();
        if (msg->functionalWrite(pkt)) {
            num_functional_writes++;
        }
    }

    // Check the stall queue and write any messages that may
    // correspond to the address in the packet.
    for (StallMsgMapType::iterator map_iter = m_stall_msg_map.begin();
         map_iter != m_stall_msg_map.end();
         ++map_iter) {

        for (std::list<MsgPtr>::iterator it = (map_iter->second).begin();
            it != (map_iter->second).end(); ++it) {

            Message *msg = (*it).get();
            if (msg->functionalWrite(pkt)) {
                num_functional_writes++;
            }
        }
    }

    return num_functional_writes;
}

MessageBuffer *
MessageBufferParams::create()
{
    return new MessageBuffer(this);
}
