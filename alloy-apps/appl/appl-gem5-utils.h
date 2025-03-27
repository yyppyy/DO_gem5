//========================================================================
// gem5-utils.h
//========================================================================
// Functions specific to gem5 (like stats, etc.). Will be treated as
// no-op in targets other than RISC-V.

#ifndef APPL_GEM5_UTILS_H
#define APPL_GEM5_UTILS_H

#include <cstdint>

namespace appl {

// toggle region of interests (ROI)
void toggle_stats(bool on);

// Returns number of clock cycles executed by the processor on which
// the hardware thread is running from an arbitrary start time in the
// past.
uint64_t get_cycles();

void register_stnt(uint64_t begin, uint64_t end);
void register_strel(uint64_t begin, uint64_t end);

void gem5_barrier_init(uint64_t n_waiters);
void gem5_barrier_wait(uint64_t bar_id);
}

#include "appl-gem5-utils.inl"

#endif
