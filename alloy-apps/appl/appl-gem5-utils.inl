//========================================================================
// gem5-utils.inl
//========================================================================

#include "appl-config.h"

namespace appl {

inline void toggle_stats(bool on)
{
#ifdef APPL_ARCH_RISCV
  __asm__ volatile ("csrw 0x7C1, %0;"
                    :
                    : "r" (on)
                    :);
  // toggle activity stats as well
  __asm__ volatile ("csrw 0x800, %0;"
                    :
                    : "r" (on)
                    :);
#endif
}

inline uint64_t get_cycles()
{
#ifdef APPL_ARCH_RISCV
  // use rdcycle pseudo-inst to read the "cycle" CSR
  uint64_t ret = 0;
  asm volatile( "rdcycle %0"
                : "=r"( ret ) // outputs
                :             // inputs
                :             // clobber list
  );
  return ret;
#else
  // unimplemented for other architecture
  return 0;
#endif
}

inline void register_stnt(uint64_t begin, uint64_t end)
{
#ifdef APPL_ARCH_RISCV
  __asm__ volatile ("csrw 0x7D0, %0;"
                    :
                    : "r" (begin)
                    :);
  __asm__ volatile ("csrw 0x7D8, %0;"
                    :
                    : "r" (end)
                    :);
#endif
}

inline void register_strel(uint64_t begin, uint64_t end)
{
#ifdef APPL_ARCH_RISCV
  __asm__ volatile ("csrw 0x7E0, %0;"
                    :
                    : "r" (begin)
                    :);
  __asm__ volatile ("csrw 0x7E8, %0;"
                    :
                    : "r" (end)
                    :);
#endif
}

inline void gem5_barrier_init(uint64_t n_waiters)
{
#ifdef APPL_ARCH_RISCV
  __asm__ volatile ("csrw 0x7F0, %0;"
                    :
                    : "r" (n_waiters)
                    :);  
#endif
}

inline void gem5_barrier_wait(uint64_t bar_id)
{
#ifdef APPL_ARCH_RISCV
  __asm__ volatile ("csrw 0x7F8, %0;"
                    :
                    : "r" (bar_id)
                    :);
  int ready = 0;
  do {
    __asm__ volatile ("csrr %0, %1;"
                    : "=r"(ready)
                    : "i"(0x790)
                    :
    );
  } while (!ready);
#endif
}

} // namespace appl