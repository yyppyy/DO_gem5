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
 * Copyright (c) 2012-2013, 2015 ARM Limited
 * Copyright (c) 2015 Advanced Micro Devices, Inc.
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
 * Copyright (c) 2003-2005 The Regents of The University of Michigan
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
 *          Kevin Lim
 */

#ifndef __SIM_SYSCALL_EMUL_HH__
#define __SIM_SYSCALL_EMUL_HH__

#if (defined(__APPLE__) || defined(__OpenBSD__) ||      \
     defined(__FreeBSD__) || defined(__CYGWIN__) ||     \
     defined(__NetBSD__))
#define NO_STAT64 1
#else
#define NO_STAT64 0
#endif

#if (defined(__APPLE__) || defined(__OpenBSD__) ||      \
     defined(__FreeBSD__) || defined(__NetBSD__))
#define NO_STATFS 1
#else
#define NO_STATFS 0
#endif

#if (defined(__APPLE__) || defined(__OpenBSD__) ||      \
     defined(__FreeBSD__) || defined(__NetBSD__))
#define NO_FALLOCATE 1
#else
#define NO_FALLOCATE 0
#endif

///
/// @file syscall_emul.hh
///
/// This file defines objects used to emulate syscalls from the target
/// application on the host machine.

#ifdef __CYGWIN32__
#include <sys/fcntl.h>

#endif
#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#if (NO_STATFS == 0)
#include <sys/statfs.h>
#else
#include <sys/mount.h>
#endif
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cerrno>
#include <memory>
#include <string>

#include "arch/generic/tlb.hh"
#include "arch/utility.hh"
#include "base/intmath.hh"
#include "base/loader/object_file.hh"
#include "base/logging.hh"
#include "base/trace.hh"
#include "base/types.hh"
#include "config/the_isa.hh"
#include "cpu/base.hh"
#include "cpu/thread_context.hh"
#include "mem/page_table.hh"
#include "params/Process.hh"
#include "sim/emul_driver.hh"
#include "sim/futex_map.hh"
#include "sim/process.hh"
#include "sim/syscall_debug_macros.hh"
#include "sim/syscall_desc.hh"
#include "sim/syscall_emul_buf.hh"
#include "sim/syscall_return.hh"

#if defined(__APPLE__) && defined(__MACH__) && !defined(CMSG_ALIGN)
#define CMSG_ALIGN(len) (((len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#endif

//////////////////////////////////////////////////////////////////////
//
// The following emulation functions are generic enough that they
// don't need to be recompiled for different emulated OS's.  They are
// defined in sim/syscall_emul.cc.
//
//////////////////////////////////////////////////////////////////////


/// Handler for unimplemented syscalls that we haven't thought about.
SyscallReturn unimplementedFunc(SyscallDesc *desc, int num,
                                Process *p, ThreadContext *tc);

/// Handler for unimplemented syscalls that we never intend to
/// implement (signal handling, etc.) and should not affect the correct
/// behavior of the program.  Print a warning only if the appropriate
/// trace flag is enabled.  Return success to the target program.
SyscallReturn ignoreFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

// Target fallocateFunc() handler.
SyscallReturn fallocateFunc(SyscallDesc *desc, int num,
                            Process *p, ThreadContext *tc);

/// Target exit() handler: terminate current context.
SyscallReturn exitFunc(SyscallDesc *desc, int num,
                       Process *p, ThreadContext *tc);

/// Target exit_group() handler: terminate simulation. (exit all threads)
SyscallReturn exitGroupFunc(SyscallDesc *desc, int num,
                       Process *p, ThreadContext *tc);

/// Target set_tid_address() handler.
SyscallReturn setTidAddressFunc(SyscallDesc *desc, int num,
                                Process *p, ThreadContext *tc);

/// Target getpagesize() handler.
SyscallReturn getpagesizeFunc(SyscallDesc *desc, int num,
                              Process *p, ThreadContext *tc);

/// Target brk() handler: set brk address.
SyscallReturn brkFunc(SyscallDesc *desc, int num,
                      Process *p, ThreadContext *tc);

/// Target close() handler.
SyscallReturn closeFunc(SyscallDesc *desc, int num,
                        Process *p, ThreadContext *tc);

/// Target lseek() handler.
SyscallReturn lseekFunc(SyscallDesc *desc, int num,
                        Process *p, ThreadContext *tc);

/// Target _llseek() handler.
SyscallReturn _llseekFunc(SyscallDesc *desc, int num,
                          Process *p, ThreadContext *tc);

/// Target munmap() handler.
SyscallReturn munmapFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

/// Target shutdown() handler.
SyscallReturn shutdownFunc(SyscallDesc *desc, int num,
                           Process *p, ThreadContext *tc);

/// Target gethostname() handler.
SyscallReturn gethostnameFunc(SyscallDesc *desc, int num,
                              Process *p, ThreadContext *tc);

/// Target getcwd() handler.
SyscallReturn getcwdFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

/// Target readlink() handler.
SyscallReturn readlinkFunc(SyscallDesc *desc, int num,
                           Process *p, ThreadContext *tc,
                           int index = 0);
SyscallReturn readlinkFunc(SyscallDesc *desc, int num,
                           Process *p, ThreadContext *tc);

/// Target unlink() handler.
SyscallReturn unlinkHelper(SyscallDesc *desc, int num,
                           Process *p, ThreadContext *tc,
                           int index);
SyscallReturn unlinkFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

/// Target link() handler
SyscallReturn linkFunc(SyscallDesc *desc, int num, Process *p,
                       ThreadContext *tc);

/// Target symlink() handler.
SyscallReturn symlinkFunc(SyscallDesc *desc, int num, Process *p,
                          ThreadContext *tc);

/// Target mkdir() handler.
SyscallReturn mkdirFunc(SyscallDesc *desc, int num,
                        Process *p, ThreadContext *tc);

/// Target mknod() handler.
SyscallReturn mknodFunc(SyscallDesc *desc, int num,
                        Process *p, ThreadContext *tc);

/// Target chdir() handler.
SyscallReturn chdirFunc(SyscallDesc *desc, int num,
                        Process *p, ThreadContext *tc);

// Target rmdir() handler.
SyscallReturn rmdirFunc(SyscallDesc *desc, int num,
                        Process *p, ThreadContext *tc);

/// Target rename() handler.
SyscallReturn renameFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);


/// Target truncate() handler.
SyscallReturn truncateFunc(SyscallDesc *desc, int num,
                           Process *p, ThreadContext *tc);


/// Target ftruncate() handler.
SyscallReturn ftruncateFunc(SyscallDesc *desc, int num,
                            Process *p, ThreadContext *tc);


/// Target truncate64() handler.
SyscallReturn truncate64Func(SyscallDesc *desc, int num,
                             Process *p, ThreadContext *tc);

/// Target ftruncate64() handler.
SyscallReturn ftruncate64Func(SyscallDesc *desc, int num,
                              Process *p, ThreadContext *tc);


/// Target umask() handler.
SyscallReturn umaskFunc(SyscallDesc *desc, int num,
                        Process *p, ThreadContext *tc);

/// Target gettid() handler.
SyscallReturn gettidFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

/// Target chown() handler.
SyscallReturn chownFunc(SyscallDesc *desc, int num,
                        Process *p, ThreadContext *tc);

/// Target setpgid() handler.
SyscallReturn setpgidFunc(SyscallDesc *desc, int num,
                          Process *p, ThreadContext *tc);

/// Target fchown() handler.
SyscallReturn fchownFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

/// Target dup() handler.
SyscallReturn dupFunc(SyscallDesc *desc, int num,
                      Process *process, ThreadContext *tc);

/// Target dup2() handler.
SyscallReturn dup2Func(SyscallDesc *desc, int num,
                       Process *process, ThreadContext *tc);

/// Target fcntl() handler.
SyscallReturn fcntlFunc(SyscallDesc *desc, int num,
                        Process *process, ThreadContext *tc);

/// Target fcntl64() handler.
SyscallReturn fcntl64Func(SyscallDesc *desc, int num,
                          Process *process, ThreadContext *tc);

/// Target setuid() handler.
SyscallReturn setuidFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

/// Target pipe() handler.
SyscallReturn pipeFunc(SyscallDesc *desc, int num,
                       Process *p, ThreadContext *tc);

/// Internal pipe() handler.
SyscallReturn pipeImpl(SyscallDesc *desc, int num, Process *p,
                       ThreadContext *tc, bool pseudoPipe);

/// Target getpid() handler.
SyscallReturn getpidFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

// Target getpeername() handler.
SyscallReturn getpeernameFunc(SyscallDesc *desc, int num,
                              Process *p, ThreadContext *tc);

// Target bind() handler.
SyscallReturn bindFunc(SyscallDesc *desc, int num,
                       Process *p, ThreadContext *tc);

// Target listen() handler.
SyscallReturn listenFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

// Target connect() handler.
SyscallReturn connectFunc(SyscallDesc *desc, int num,
                          Process *p, ThreadContext *tc);

#if defined(SYS_getdents)
// Target getdents() handler.
SyscallReturn getdentsFunc(SyscallDesc *desc, int num,
                           Process *p, ThreadContext *tc);
#endif

#if defined(SYS_getdents64)
// Target getdents() handler.
SyscallReturn getdents64Func(SyscallDesc *desc, int num,
                           Process *p, ThreadContext *tc);
#endif

// Target sendto() handler.
SyscallReturn sendtoFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

// Target recvfrom() handler.
SyscallReturn recvfromFunc(SyscallDesc *desc, int num,
                           Process *p, ThreadContext *tc);

// Target recvmsg() handler.
SyscallReturn recvmsgFunc(SyscallDesc *desc, int num,
                          Process *p, ThreadContext *tc);

// Target sendmsg() handler.
SyscallReturn sendmsgFunc(SyscallDesc *desc, int num,
                          Process *p, ThreadContext *tc);

// Target getuid() handler.
SyscallReturn getuidFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

/// Target getgid() handler.
SyscallReturn getgidFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);

/// Target getppid() handler.
SyscallReturn getppidFunc(SyscallDesc *desc, int num,
                          Process *p, ThreadContext *tc);

/// Target geteuid() handler.
SyscallReturn geteuidFunc(SyscallDesc *desc, int num,
                          Process *p, ThreadContext *tc);

/// Target getegid() handler.
SyscallReturn getegidFunc(SyscallDesc *desc, int num,
                          Process *p, ThreadContext *tc);

/// Target access() handler
SyscallReturn accessFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc);
SyscallReturn accessFunc(SyscallDesc *desc, int num,
                         Process *p, ThreadContext *tc,
                         int index);

// Target getsockopt() handler.
SyscallReturn getsockoptFunc(SyscallDesc *desc, int num,
                             Process *p, ThreadContext *tc);

// Target setsockopt() handler.
SyscallReturn setsockoptFunc(SyscallDesc *desc, int num,
                             Process *p, ThreadContext *tc);

// Target getsockname() handler.
SyscallReturn getsocknameFunc(SyscallDesc *desc, int num,
                              Process *p, ThreadContext *tc);

/// Futex system call
/// Implemented by Daniel Sanchez
/// Used by printf's in multi-threaded apps
template <class OS>
SyscallReturn
futexFunc(SyscallDesc *desc, int callnum, Process *process,
          ThreadContext *tc)
{
    using namespace std;

    int index = 0;
    Addr uaddr = process->getSyscallArg(tc, index);
    int op = process->getSyscallArg(tc, index);
    int val = process->getSyscallArg(tc, index);
    int timeout M5_VAR_USED = process->getSyscallArg(tc, index);
    Addr uaddr2 M5_VAR_USED = process->getSyscallArg(tc, index);
    int val3 = process->getSyscallArg(tc, index);

    /*
     * Unsupported option that does not affect the correctness of the
     * application. This is a performance optimization utilized by Linux.
     */
    op &= ~OS::TGT_FUTEX_PRIVATE_FLAG;
    op &= ~OS::TGT_FUTEX_CLOCK_REALTIME_FLAG;

    FutexMap &futex_map = tc->getSystemPtr()->futexMap;

    if (OS::TGT_FUTEX_WAIT == op || OS::TGT_FUTEX_WAIT_BITSET == op) {
        // Ensure futex system call accessed atomically.
        BufferArg buf(uaddr, sizeof(int));
        buf.copyIn(tc->getMemProxy());
        int mem_val = *(int*)buf.bufferPtr();

        /*
         * The value in memory at uaddr is not equal with the expected val
         * (a different thread must have changed it before the system call was
         * invoked). In this case, we need to throw an error.
         */
        if (val != mem_val)
            return -OS::TGT_EWOULDBLOCK;

        if (OS::TGT_FUTEX_WAIT) {
            futex_map.suspend(uaddr, process->tgid(), tc);
        } else {
            futex_map.suspend_bitset(uaddr, process->tgid(), tc, val3);
        }

        return 0;
    } else if (OS::TGT_FUTEX_WAKE == op) {
        return futex_map.wakeup(uaddr, process->tgid(), val);
    } else if (OS::TGT_FUTEX_WAKE_BITSET == op) {
        return futex_map.wakeup_bitset(uaddr, process->tgid(), val3);
    } else if (OS::TGT_FUTEX_REQUEUE == op ||
               OS::TGT_FUTEX_CMP_REQUEUE == op) {

        // Ensure futex system call accessed atomically.
        BufferArg buf(uaddr, sizeof(int));
        buf.copyIn(tc->getMemProxy());
        int mem_val = *(int*)buf.bufferPtr();
        /*
         * For CMP_REQUEUE, the whole operation is only started only if
         * val3 is still the value of the futex pointed to by uaddr.
         */
        if (OS::TGT_FUTEX_CMP_REQUEUE && val3 != mem_val)
            return -OS::TGT_EWOULDBLOCK;
        return futex_map.requeue(uaddr, process->tgid(), val, timeout, uaddr2);
    } else if (OS::TGT_FUTEX_WAKE_OP == op) {
        /*
         * The FUTEX_WAKE_OP operation is equivalent to executing the
         * following code atomically and totally ordered with respect to
         * other futex operations on any of the two supplied futex words:
         *
         *   int oldval = *(int *) addr2;
         *   *(int *) addr2 = oldval op oparg;
         *   futex(addr1, FUTEX_WAKE, val, 0, 0, 0);
         *   if (oldval cmp cmparg)
         *        futex(addr2, FUTEX_WAKE, val2, 0, 0, 0);
         *
         * (op, oparg, cmp, cmparg are encoded in val3)
         *
         * +---+---+-----------+-----------+
         * |op |cmp|   oparg   |  cmparg   |
         * +---+---+-----------+-----------+
         *   4   4       12          12    <== # of bits
         *
         * reference: http://man7.org/linux/man-pages/man2/futex.2.html
         *
         */
        // get value from simulated-space
        BufferArg buf(uaddr2, sizeof(int));
        buf.copyIn(tc->getMemProxy());
        int oldval = *(int*)buf.bufferPtr();
        int newval = oldval;
        // extract op, oparg, cmp, cmparg from val3
        int wake_cmparg =  val3 & 0xfff;
        int wake_oparg  = (val3 & 0xfff000)   >> 12;
        int wake_cmp    = (val3 & 0xf000000)  >> 24;
        int wake_op     = (val3 & 0xf0000000) >> 28;
        if ((wake_op & OS::TGT_FUTEX_OP_ARG_SHIFT) >> 3 == 1)
            wake_oparg = (1 << wake_oparg);
        wake_op &= ~OS::TGT_FUTEX_OP_ARG_SHIFT;
        // perform operation on the value of the second futex
        if (wake_op == OS::TGT_FUTEX_OP_SET)
            newval = wake_oparg;
        else if (wake_op == OS::TGT_FUTEX_OP_ADD)
            newval += wake_oparg;
        else if (wake_op == OS::TGT_FUTEX_OP_OR)
            newval |= wake_oparg;
        else if (wake_op == OS::TGT_FUTEX_OP_ANDN)
            newval &= ~wake_oparg;
        else if (wake_op == OS::TGT_FUTEX_OP_XOR)
            newval ^= wake_oparg;
        // copy updated value back to simulated-space
        *(int*)buf.bufferPtr() = newval;
        buf.copyOut(tc->getMemProxy());
        // perform the first wake-up
        int woken1 = futex_map.wakeup(uaddr, process->tgid(), val);
        int woken2 = 0;
        // calculate the condition of the second wake-up
        bool is_wake2 = false;
        if (wake_cmp == OS::TGT_FUTEX_OP_CMP_EQ)
            is_wake2 = oldval == wake_cmparg;
        else if (wake_cmp == OS::TGT_FUTEX_OP_CMP_NE)
            is_wake2 = oldval != wake_cmparg;
        else if (wake_cmp == OS::TGT_FUTEX_OP_CMP_LT)
            is_wake2 = oldval < wake_cmparg;
        else if (wake_cmp == OS::TGT_FUTEX_OP_CMP_LE)
            is_wake2 = oldval <= wake_cmparg;
        else if (wake_cmp == OS::TGT_FUTEX_OP_CMP_GT)
            is_wake2 = oldval > wake_cmparg;
        else if (wake_cmp == OS::TGT_FUTEX_OP_CMP_GE)
            is_wake2 = oldval >= wake_cmparg;
        // perform the second wake-up
        if (is_wake2)
            woken2 = futex_map.wakeup(uaddr2, process->tgid(), timeout);

        return woken1 + woken2;
    }
    warn("futex: op %d not implemented; ignoring.", op);
    return -ENOSYS;
}


/// Pseudo Funcs  - These functions use a different return convension,
/// returning a second value in a register other than the normal return register
SyscallReturn pipePseudoFunc(SyscallDesc *desc, int num,
                             Process *process, ThreadContext *tc);

/// Target getpidPseudo() handler.
SyscallReturn getpidPseudoFunc(SyscallDesc *desc, int num,
                               Process *p, ThreadContext *tc);

/// Target getuidPseudo() handler.
SyscallReturn getuidPseudoFunc(SyscallDesc *desc, int num,
                               Process *p, ThreadContext *tc);

/// Target getgidPseudo() handler.
SyscallReturn getgidPseudoFunc(SyscallDesc *desc, int num,
                               Process *p, ThreadContext *tc);


/// A readable name for 1,000,000, for converting microseconds to seconds.
const int one_million = 1000000;
/// A readable name for 1,000,000,000, for converting nanoseconds to seconds.
const int one_billion = 1000000000;

/// Approximate seconds since the epoch (1/1/1970).  About a billion,
/// by my reckoning.  We want to keep this a constant (not use the
/// real-world time) to keep simulations repeatable.
const unsigned seconds_since_epoch = 1000000000;

/// Helper function to convert current elapsed time to seconds and
/// microseconds.
template <class T1, class T2>
void
getElapsedTimeMicro(T1 &sec, T2 &usec)
{
    uint64_t elapsed_usecs = curTick() / SimClock::Int::us;
    sec = elapsed_usecs / one_million;
    usec = elapsed_usecs % one_million;
}

/// Helper function to convert current elapsed time to seconds and
/// nanoseconds.
template <class T1, class T2>
void
getElapsedTimeNano(T1 &sec, T2 &nsec)
{
    uint64_t elapsed_nsecs = curTick() / SimClock::Int::ns;
    sec = elapsed_nsecs / one_billion;
    nsec = elapsed_nsecs % one_billion;
}

//////////////////////////////////////////////////////////////////////
//
// The following emulation functions are generic, but need to be
// templated to account for differences in types, constants, etc.
//
//////////////////////////////////////////////////////////////////////

    typedef struct statfs hst_statfs;
#if NO_STAT64
    typedef struct stat hst_stat;
    typedef struct stat hst_stat64;
#else
    typedef struct stat hst_stat;
    typedef struct stat64 hst_stat64;
#endif

//// Helper function to convert a host stat buffer to a target stat
//// buffer.  Also copies the target buffer out to the simulated
//// memory space.  Used by stat(), fstat(), and lstat().

template <typename target_stat, typename host_stat>
void
convertStatBuf(target_stat &tgt, host_stat *host, bool fakeTTY = false)
{
    using namespace TheISA;

    if (fakeTTY)
        tgt->st_dev = 0xA;
    else
        tgt->st_dev = host->st_dev;
    tgt->st_dev = TheISA::htog(tgt->st_dev);
    tgt->st_ino = host->st_ino;
    tgt->st_ino = TheISA::htog(tgt->st_ino);
    tgt->st_mode = host->st_mode;
    if (fakeTTY) {
        // Claim to be a character device
        tgt->st_mode &= ~S_IFMT;    // Clear S_IFMT
        tgt->st_mode |= S_IFCHR;    // Set S_IFCHR
    }
    tgt->st_mode = TheISA::htog(tgt->st_mode);
    tgt->st_nlink = host->st_nlink;
    tgt->st_nlink = TheISA::htog(tgt->st_nlink);
    tgt->st_uid = host->st_uid;
    tgt->st_uid = TheISA::htog(tgt->st_uid);
    tgt->st_gid = host->st_gid;
    tgt->st_gid = TheISA::htog(tgt->st_gid);
    if (fakeTTY)
        tgt->st_rdev = 0x880d;
    else
        tgt->st_rdev = host->st_rdev;
    tgt->st_rdev = TheISA::htog(tgt->st_rdev);
    tgt->st_size = host->st_size;
    tgt->st_size = TheISA::htog(tgt->st_size);
    tgt->st_atimeX = host->st_atime;
    tgt->st_atimeX = TheISA::htog(tgt->st_atimeX);
    tgt->st_mtimeX = host->st_mtime;
    tgt->st_mtimeX = TheISA::htog(tgt->st_mtimeX);
    tgt->st_ctimeX = host->st_ctime;
    tgt->st_ctimeX = TheISA::htog(tgt->st_ctimeX);
    // Force the block size to be 8KB. This helps to ensure buffered io works
    // consistently across different hosts.
    tgt->st_blksize = 0x2000;
    tgt->st_blksize = TheISA::htog(tgt->st_blksize);
    tgt->st_blocks = host->st_blocks;
    tgt->st_blocks = TheISA::htog(tgt->st_blocks);
}

// Same for stat64

template <typename target_stat, typename host_stat64>
void
convertStat64Buf(target_stat &tgt, host_stat64 *host, bool fakeTTY = false)
{
    using namespace TheISA;

    convertStatBuf<target_stat, host_stat64>(tgt, host, fakeTTY);
#if defined(STAT_HAVE_NSEC)
    tgt->st_atime_nsec = host->st_atime_nsec;
    tgt->st_atime_nsec = TheISA::htog(tgt->st_atime_nsec);
    tgt->st_mtime_nsec = host->st_mtime_nsec;
    tgt->st_mtime_nsec = TheISA::htog(tgt->st_mtime_nsec);
    tgt->st_ctime_nsec = host->st_ctime_nsec;
    tgt->st_ctime_nsec = TheISA::htog(tgt->st_ctime_nsec);
#else
    tgt->st_atime_nsec = 0;
    tgt->st_mtime_nsec = 0;
    tgt->st_ctime_nsec = 0;
#endif
}

// Here are a couple of convenience functions
template<class OS>
void
copyOutStatBuf(SETranslatingPortProxy &mem, Addr addr,
               hst_stat *host, bool fakeTTY = false)
{
    typedef TypedBufferArg<typename OS::tgt_stat> tgt_stat_buf;
    tgt_stat_buf tgt(addr);
    convertStatBuf<tgt_stat_buf, hst_stat>(tgt, host, fakeTTY);
    tgt.copyOut(mem);
}

template<class OS>
void
copyOutStat64Buf(SETranslatingPortProxy &mem, Addr addr,
                 hst_stat64 *host, bool fakeTTY = false)
{
    typedef TypedBufferArg<typename OS::tgt_stat64> tgt_stat_buf;
    tgt_stat_buf tgt(addr);
    convertStat64Buf<tgt_stat_buf, hst_stat64>(tgt, host, fakeTTY);
    tgt.copyOut(mem);
}

template <class OS>
void
copyOutStatfsBuf(SETranslatingPortProxy &mem, Addr addr,
                 hst_statfs *host)
{
    TypedBufferArg<typename OS::tgt_statfs> tgt(addr);

    tgt->f_type = TheISA::htog(host->f_type);
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    tgt->f_bsize = TheISA::htog(host->f_iosize);
#else
    tgt->f_bsize = TheISA::htog(host->f_bsize);
#endif
    tgt->f_blocks = TheISA::htog(host->f_blocks);
    tgt->f_bfree = TheISA::htog(host->f_bfree);
    tgt->f_bavail = TheISA::htog(host->f_bavail);
    tgt->f_files = TheISA::htog(host->f_files);
    tgt->f_ffree = TheISA::htog(host->f_ffree);
    memcpy(&tgt->f_fsid, &host->f_fsid, sizeof(host->f_fsid));
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    tgt->f_namelen = TheISA::htog(host->f_namemax);
    tgt->f_frsize = TheISA::htog(host->f_bsize);
#elif defined(__APPLE__)
    tgt->f_namelen = 0;
    tgt->f_frsize = 0;
#else
    tgt->f_namelen = TheISA::htog(host->f_namelen);
    tgt->f_frsize = TheISA::htog(host->f_frsize);
#endif
#if defined(__linux__)
    memcpy(&tgt->f_spare, &host->f_spare, sizeof(host->f_spare));
#else
    /*
     * The fields are different sizes per OS. Don't bother with
     * f_spare or f_reserved on non-Linux for now.
     */
    memset(&tgt->f_spare, 0, sizeof(tgt->f_spare));
#endif

    tgt.copyOut(mem);
}

/// Target ioctl() handler.  For the most part, programs call ioctl()
/// only to find out if their stdout is a tty, to determine whether to
/// do line or block buffering.  We always claim that output fds are
/// not TTYs to provide repeatable results.
template <class OS>
SyscallReturn
ioctlFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    unsigned req = p->getSyscallArg(tc, index);

    DPRINTF(SyscallVerbose, "ioctl(%d, 0x%x, ...)\n", tgt_fd, req);

    if (OS::isTtyReq(req))
        return -ENOTTY;

    auto dfdp = std::dynamic_pointer_cast<DeviceFDEntry>((*p->fds)[tgt_fd]);
    if (!dfdp)
        return -EBADF;

    /**
     * If the driver is valid, issue the ioctl through it. Otherwise,
     * there's an implicit assumption that the device is a TTY type and we
     * return that we do not have a valid TTY.
     */
    EmulatedDriver *emul_driver = dfdp->getDriver();
    if (emul_driver)
        return emul_driver->ioctl(p, tc, req);

    /**
     * For lack of a better return code, return ENOTTY. Ideally, we should
     * return something better here, but at least we issue the warning.
     */
    warn("Unsupported ioctl call (return ENOTTY): ioctl(%d, 0x%x, ...) @ \n",
         tgt_fd, req, tc->pcState());
    return -ENOTTY;
}

template <class OS>
SyscallReturn
openImpl(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc,
         bool isopenat)
{
    int index = 0;
    int tgt_dirfd = -1;

    /**
     * If using the openat variant, read in the target directory file
     * descriptor from the simulated process.
     */
    if (isopenat)
        tgt_dirfd = p->getSyscallArg(tc, index);

    /**
     * Retrieve the simulated process' memory proxy and then read in the path
     * string from that memory space into the host's working memory space.
     */
    std::string path;
    if (!tc->getMemProxy().tryReadString(path, p->getSyscallArg(tc, index)))
        return -EFAULT;

#ifdef __CYGWIN32__
    int host_flags = O_BINARY;
#else
    int host_flags = 0;
#endif
    /**
     * Translate target flags into host flags. Flags exist which are not
     * ported between architectures which can cause check failures.
     */
    int tgt_flags = p->getSyscallArg(tc, index);
    for (int i = 0; i < OS::NUM_OPEN_FLAGS; i++) {
        if (tgt_flags & OS::openFlagTable[i].tgtFlag) {
            tgt_flags &= ~OS::openFlagTable[i].tgtFlag;
            host_flags |= OS::openFlagTable[i].hostFlag;
        }
    }
    if (tgt_flags) {
        warn("open%s: cannot decode flags 0x%x",
             isopenat ? "at" : "", tgt_flags);
    }
#ifdef __CYGWIN32__
    host_flags |= O_BINARY;
#endif

    int mode = p->getSyscallArg(tc, index);

    /**
     * If the simulated process called open or openat with AT_FDCWD specified,
     * take the current working directory value which was passed into the
     * process class as a Python parameter and append the current path to
     * create a full path.
     * Otherwise, openat with a valid target directory file descriptor has
     * been called. If the path option, which was passed in as a parameter,
     * is not absolute, retrieve the directory file descriptor's path and
     * prepend it to the path passed in as a parameter.
     * In every case, we should have a full path (which is relevant to the
     * host) to work with after this block has been passed.
     */
    if (!isopenat || (isopenat && tgt_dirfd == OS::TGT_AT_FDCWD)) {
        path = p->fullPath(path);
    } else if (!startswith(path, "/")) {
        std::shared_ptr<FDEntry> fdep = ((*p->fds)[tgt_dirfd]);
        auto ffdp = std::dynamic_pointer_cast<FileFDEntry>(fdep);
        if (!ffdp)
            return -EBADF;
        path.insert(0, ffdp->getFileName() + "/");
    }

    /**
     * Since this is an emulated environment, we create pseudo file
     * descriptors for device requests that have been registered with
     * the process class through Python; this allows us to create a file
     * descriptor for subsequent ioctl or mmap calls.
     */
    if (startswith(path, "/dev/")) {
        std::string filename = path.substr(strlen("/dev/"));
        EmulatedDriver *drv = p->findDriver(filename);
        if (drv) {
            DPRINTF_SYSCALL(Verbose, "open%s: passing call to "
                            "driver open with path[%s]\n",
                            isopenat ? "at" : "", path.c_str());
            return drv->open(p, tc, mode, host_flags);
        }
        /**
         * Fall through here for pass through to host devices, such
         * as /dev/zero
         */
    }

    /**
     * Some special paths and files cannot be called on the host and need
     * to be handled as special cases inside the simulator.
     * If the full path that was created above does not match any of the
     * special cases, pass it through to the open call on the host to let
     * the host open the file on our behalf.
     * If the host cannot open the file, return the host's error code back
     * through the system call to the simulated process.
     */
    int sim_fd = -1;
    std::vector<std::string> special_paths =
            { "/proc/", "/system/", "/sys/", "/platform/", "/etc/passwd" };
    for (auto entry : special_paths) {
        if (startswith(path, entry))
            sim_fd = OS::openSpecialFile(path, p, tc);
    }
    if (sim_fd == -1) {
        sim_fd = open(path.c_str(), host_flags, mode);
    }
    if (sim_fd == -1) {
        int local = -errno;
        DPRINTF_SYSCALL(Verbose, "open%s: failed -> path:%s\n",
                        isopenat ? "at" : "", path.c_str());
        return local;
    }

    /**
     * The file was opened successfully and needs to be recorded in the
     * process' file descriptor array so that it can be retrieved later.
     * The target file descriptor that is chosen will be the lowest unused
     * file descriptor.
     * Return the indirect target file descriptor back to the simulated
     * process to act as a handle for the opened file.
     */
    auto ffdp = std::make_shared<FileFDEntry>(sim_fd, host_flags, path, 0);
    int tgt_fd = p->fds->allocFD(ffdp);
    DPRINTF_SYSCALL(Verbose, "open%s: sim_fd[%d], target_fd[%d] -> path:%s\n",
                    isopenat ? "at" : "", sim_fd, tgt_fd, path.c_str());
    return tgt_fd;
}

/// Target open() handler.
template <class OS>
SyscallReturn
openFunc(SyscallDesc *desc, int callnum, Process *process,
         ThreadContext *tc)
{
    return openImpl<OS>(desc, callnum, process, tc, false);
}

/// Target openat() handler.
template <class OS>
SyscallReturn
openatFunc(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
    return openImpl<OS>(desc, callnum, process, tc, true);
}

/// Target unlinkat() handler.
template <class OS>
SyscallReturn
unlinkatFunc(SyscallDesc *desc, int callnum, Process *process,
             ThreadContext *tc)
{
    int index = 0;
    int dirfd = process->getSyscallArg(tc, index);
    if (dirfd != OS::TGT_AT_FDCWD)
        warn("unlinkat: first argument not AT_FDCWD; unlikely to work");

    return unlinkHelper(desc, callnum, process, tc, 1);
}

/// Target facessat() handler
template <class OS>
SyscallReturn
faccessatFunc(SyscallDesc *desc, int callnum, Process *process,
              ThreadContext *tc)
{
    int index = 0;
    int dirfd = process->getSyscallArg(tc, index);
    if (dirfd != OS::TGT_AT_FDCWD)
        warn("faccessat: first argument not AT_FDCWD; unlikely to work");
    return accessFunc(desc, callnum, process, tc, 1);
}

/// Target readlinkat() handler
template <class OS>
SyscallReturn
readlinkatFunc(SyscallDesc *desc, int callnum, Process *process,
               ThreadContext *tc)
{
    int index = 0;
    int dirfd = process->getSyscallArg(tc, index);
    if (dirfd != OS::TGT_AT_FDCWD)
        warn("openat: first argument not AT_FDCWD; unlikely to work");
    return readlinkFunc(desc, callnum, process, tc, 1);
}

/// Target renameat() handler.
template <class OS>
SyscallReturn
renameatFunc(SyscallDesc *desc, int callnum, Process *process,
             ThreadContext *tc)
{
    int index = 0;

    int olddirfd = process->getSyscallArg(tc, index);
    if (olddirfd != OS::TGT_AT_FDCWD)
        warn("renameat: first argument not AT_FDCWD; unlikely to work");

    std::string old_name;

    if (!tc->getMemProxy().tryReadString(old_name,
                                         process->getSyscallArg(tc, index)))
        return -EFAULT;

    int newdirfd = process->getSyscallArg(tc, index);
    if (newdirfd != OS::TGT_AT_FDCWD)
        warn("renameat: third argument not AT_FDCWD; unlikely to work");

    std::string new_name;

    if (!tc->getMemProxy().tryReadString(new_name,
                                         process->getSyscallArg(tc, index)))
        return -EFAULT;

    // Adjust path for current working directory
    old_name = process->fullPath(old_name);
    new_name = process->fullPath(new_name);

    int result = rename(old_name.c_str(), new_name.c_str());
    return (result == -1) ? -errno : result;
}

/// Target sysinfo() handler.
template <class OS>
SyscallReturn
sysinfoFunc(SyscallDesc *desc, int callnum, Process *process,
            ThreadContext *tc)
{

    int index = 0;
    TypedBufferArg<typename OS::tgt_sysinfo>
        sysinfo(process->getSyscallArg(tc, index));

    sysinfo->uptime = seconds_since_epoch;
    sysinfo->totalram = process->system->memSize();
    sysinfo->mem_unit = 1;

    sysinfo.copyOut(tc->getMemProxy());

    return 0;
}

/// Target chmod() handler.
template <class OS>
SyscallReturn
chmodFunc(SyscallDesc *desc, int callnum, Process *process,
          ThreadContext *tc)
{
    std::string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path,
                process->getSyscallArg(tc, index))) {
        return -EFAULT;
    }

    uint32_t mode = process->getSyscallArg(tc, index);
    mode_t hostMode = 0;

    // XXX translate mode flags via OS::something???
    hostMode = mode;

    // Adjust path for current working directory
    path = process->fullPath(path);

    // do the chmod
    int result = chmod(path.c_str(), hostMode);
    if (result < 0)
        return -errno;

    return 0;
}

template <class OS>
SyscallReturn
pollFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    Addr fdsPtr = p->getSyscallArg(tc, index);
    int nfds = p->getSyscallArg(tc, index);
    int tmout = p->getSyscallArg(tc, index);

    BufferArg fdsBuf(fdsPtr, sizeof(struct pollfd) * nfds);
    fdsBuf.copyIn(tc->getMemProxy());

    /**
     * Record the target file descriptors in a local variable. We need to
     * replace them with host file descriptors but we need a temporary copy
     * for later. Afterwards, replace each target file descriptor in the
     * poll_fd array with its host_fd.
     */
    int temp_tgt_fds[nfds];
    for (index = 0; index < nfds; index++) {
        temp_tgt_fds[index] = ((struct pollfd *)fdsBuf.bufferPtr())[index].fd;
        auto tgt_fd = temp_tgt_fds[index];
        auto hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[tgt_fd]);
        if (!hbfdp)
            return -EBADF;
        auto host_fd = hbfdp->getSimFD();
        ((struct pollfd *)fdsBuf.bufferPtr())[index].fd = host_fd;
    }

    /**
     * We cannot allow an infinite poll to occur or it will inevitably cause
     * a deadlock in the gem5 simulator with clone. We must pass in tmout with
     * a non-negative value, however it also makes no sense to poll on the
     * underlying host for any other time than tmout a zero timeout.
     */
    int status;
    if (tmout < 0) {
        status = poll((struct pollfd *)fdsBuf.bufferPtr(), nfds, 0);
        if (status == 0) {
            /**
             * If blocking indefinitely, check the signal list to see if a
             * signal would break the poll out of the retry cycle and try
             * to return the signal interrupt instead.
             */
            System *sysh = tc->getSystemPtr();
            std::list<BasicSignal>::iterator it;
            for (it=sysh->signalList.begin(); it!=sysh->signalList.end(); it++)
                if (it->receiver == p)
                    return -EINTR;
            return SyscallReturn::retry();
        }
    } else
        status = poll((struct pollfd *)fdsBuf.bufferPtr(), nfds, 0);

    if (status == -1)
        return -errno;

    /**
     * Replace each host_fd in the returned poll_fd array with its original
     * target file descriptor.
     */
    for (index = 0; index < nfds; index++) {
        auto tgt_fd = temp_tgt_fds[index];
        ((struct pollfd *)fdsBuf.bufferPtr())[index].fd = tgt_fd;
    }

    /**
     * Copy out the pollfd struct because the host may have updated fields
     * in the structure.
     */
    fdsBuf.copyOut(tc->getMemProxy());

    return status;
}

/// Target fchmod() handler.
template <class OS>
SyscallReturn
fchmodFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    uint32_t mode = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    mode_t hostMode = mode;

    int result = fchmod(sim_fd, hostMode);

    return (result < 0) ? -errno : 0;
}

/// Target mremap() handler.
template <class OS>
SyscallReturn
mremapFunc(SyscallDesc *desc, int callnum, Process *process, ThreadContext *tc)
{
    DPRINTF_SYSCALL(Verbose, "tc[%p] in syscall mremap\n", tc);
    int index = 0;
    Addr start = process->getSyscallArg(tc, index);
    uint64_t old_length = process->getSyscallArg(tc, index);
    uint64_t new_length = process->getSyscallArg(tc, index);
    uint64_t flags = process->getSyscallArg(tc, index);
    uint64_t provided_address = 0;
    bool use_provided_address = flags & OS::TGT_MREMAP_FIXED;

    if (use_provided_address)
        provided_address = process->getSyscallArg(tc, index);

    if ((start % TheISA::PageBytes != 0) ||
        (provided_address % TheISA::PageBytes != 0)) {
        warn("mremap failing: arguments not page aligned");
        return -EINVAL;
    }

    new_length = roundUp(new_length, TheISA::PageBytes);

    if (new_length > old_length) {
        std::shared_ptr<MemState> mem_state = process->memState;
        Addr mmap_end = mem_state->getMmapEnd();

        if ((start + old_length) == mmap_end &&
            (!use_provided_address || provided_address == start)) {
            // This case cannot occur when growing downward, as
            // start is greater than or equal to mmap_end.
            uint64_t diff = new_length - old_length;
            process->allocateMemThreadCtx(mmap_end, diff, false, tc);
            mem_state->setMmapEnd(mmap_end + diff);
            return start;
        } else {
            if (!use_provided_address && !(flags & OS::TGT_MREMAP_MAYMOVE)) {
                warn("can't remap here and MREMAP_MAYMOVE flag not set\n");
                return -ENOMEM;
            } else {
                uint64_t new_start = provided_address;
                if (!use_provided_address) {
                    new_start = process->mmapGrowsDown() ?
                                mmap_end - new_length : mmap_end;
                    mmap_end = process->mmapGrowsDown() ?
                               new_start : mmap_end + new_length;
                    mem_state->setMmapEnd(mmap_end);
                }

                process->pTable->remap(start, old_length, new_start);
                warn("mremapping to new vaddr %08p-%08p, adding %d\n",
                     new_start, new_start + new_length,
                     new_length - old_length);
                // add on the remaining unallocated pages
                process->allocateMemThreadCtx(new_start + old_length,
                                     new_length - old_length,
                                     use_provided_address /* clobber */, tc);
                if (use_provided_address &&
                    ((new_start + new_length > mem_state->getMmapEnd() &&
                      !process->mmapGrowsDown()) ||
                    (new_start < mem_state->getMmapEnd() &&
                      process->mmapGrowsDown()))) {
                    // something fishy going on here, at least notify the user
                    // @todo: increase mmap_end?
                    warn("mmap region limit exceeded with MREMAP_FIXED\n");
                }
                warn("returning %08p as start\n", new_start);
                return new_start;
            }
        }
    } else {
        if (use_provided_address && provided_address != start)
            process->pTable->remap(start, new_length, provided_address);
        process->pTable->unmap(start + new_length, old_length - new_length);
        return use_provided_address ? provided_address : start;
    }
}

/// Target stat() handler.
template <class OS>
SyscallReturn
statFunc(SyscallDesc *desc, int callnum, Process *process,
         ThreadContext *tc)
{
    std::string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path,
                process->getSyscallArg(tc, index))) {
        return -EFAULT;
    }
    Addr bufPtr = process->getSyscallArg(tc, index);

    // Adjust path for current working directory
    path = process->fullPath(path);

    struct stat hostBuf;
    int result = stat(path.c_str(), &hostBuf);

    if (result < 0)
        return -errno;

    copyOutStatBuf<OS>(tc->getMemProxy(), bufPtr, &hostBuf);

    return 0;
}


/// Target stat64() handler.
template <class OS>
SyscallReturn
stat64Func(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
    std::string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path,
                process->getSyscallArg(tc, index)))
        return -EFAULT;
    Addr bufPtr = process->getSyscallArg(tc, index);

    // Adjust path for current working directory
    path = process->fullPath(path);

#if NO_STAT64
    struct stat  hostBuf;
    int result = stat(path.c_str(), &hostBuf);
#else
    struct stat64 hostBuf;
    int result = stat64(path.c_str(), &hostBuf);
#endif

    if (result < 0)
        return -errno;

    copyOutStat64Buf<OS>(tc->getMemProxy(), bufPtr, &hostBuf);

    return 0;
}


/// Target fstatat64() handler.
template <class OS>
SyscallReturn
fstatat64Func(SyscallDesc *desc, int callnum, Process *process,
              ThreadContext *tc)
{
    int index = 0;
    int dirfd = process->getSyscallArg(tc, index);
    if (dirfd != OS::TGT_AT_FDCWD)
        warn("fstatat64: first argument not AT_FDCWD; unlikely to work");

    std::string path;
    if (!tc->getMemProxy().tryReadString(path,
                process->getSyscallArg(tc, index)))
        return -EFAULT;
    Addr bufPtr = process->getSyscallArg(tc, index);

    // Adjust path for current working directory
    path = process->fullPath(path);

#if NO_STAT64
    struct stat  hostBuf;
    int result = stat(path.c_str(), &hostBuf);
#else
    struct stat64 hostBuf;
    int result = stat64(path.c_str(), &hostBuf);
#endif

    if (result < 0)
        return -errno;

    copyOutStat64Buf<OS>(tc->getMemProxy(), bufPtr, &hostBuf);

    return 0;
}


/// Target fstat64() handler.
template <class OS>
SyscallReturn
fstat64Func(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    Addr bufPtr = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

#if NO_STAT64
    struct stat  hostBuf;
    int result = fstat(sim_fd, &hostBuf);
#else
    struct stat64  hostBuf;
    int result = fstat64(sim_fd, &hostBuf);
#endif

    if (result < 0)
        return -errno;

    copyOutStat64Buf<OS>(tc->getMemProxy(), bufPtr, &hostBuf, (sim_fd == 1));

    return 0;
}


/// Target lstat() handler.
template <class OS>
SyscallReturn
lstatFunc(SyscallDesc *desc, int callnum, Process *process,
          ThreadContext *tc)
{
    std::string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path,
                process->getSyscallArg(tc, index))) {
        return -EFAULT;
    }
    Addr bufPtr = process->getSyscallArg(tc, index);

    // Adjust path for current working directory
    path = process->fullPath(path);

    struct stat hostBuf;
    int result = lstat(path.c_str(), &hostBuf);

    if (result < 0)
        return -errno;

    copyOutStatBuf<OS>(tc->getMemProxy(), bufPtr, &hostBuf);

    return 0;
}

/// Target lstat64() handler.
template <class OS>
SyscallReturn
lstat64Func(SyscallDesc *desc, int callnum, Process *process,
            ThreadContext *tc)
{
    std::string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path,
                process->getSyscallArg(tc, index))) {
        return -EFAULT;
    }
    Addr bufPtr = process->getSyscallArg(tc, index);

    // Adjust path for current working directory
    path = process->fullPath(path);

#if NO_STAT64
    struct stat hostBuf;
    int result = lstat(path.c_str(), &hostBuf);
#else
    struct stat64 hostBuf;
    int result = lstat64(path.c_str(), &hostBuf);
#endif

    if (result < 0)
        return -errno;

    copyOutStat64Buf<OS>(tc->getMemProxy(), bufPtr, &hostBuf);

    return 0;
}

/// Target fstat() handler.
template <class OS>
SyscallReturn
fstatFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    Addr bufPtr = p->getSyscallArg(tc, index);

    DPRINTF_SYSCALL(Verbose, "fstat(%d, ...)\n", tgt_fd);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    struct stat hostBuf;
    int result = fstat(sim_fd, &hostBuf);

    if (result < 0)
        return -errno;

    copyOutStatBuf<OS>(tc->getMemProxy(), bufPtr, &hostBuf, (sim_fd == 1));

    return 0;
}

/// Target statfs() handler.
template <class OS>
SyscallReturn
statfsFunc(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
#if NO_STATFS
    warn("Host OS cannot support calls to statfs. Ignoring syscall");
#else
    std::string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path,
                process->getSyscallArg(tc, index))) {
        return -EFAULT;
    }
    Addr bufPtr = process->getSyscallArg(tc, index);

    // Adjust path for current working directory
    path = process->fullPath(path);

    struct statfs hostBuf;
    int result = statfs(path.c_str(), &hostBuf);

    if (result < 0)
        return -errno;

    copyOutStatfsBuf<OS>(tc->getMemProxy(), bufPtr, &hostBuf);
#endif
    return 0;
}

template <class OS>
SyscallReturn
cloneFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    int index = 0;

    RegVal flags = p->getSyscallArg(tc, index);
    RegVal newStack = p->getSyscallArg(tc, index);
    Addr ptidPtr = p->getSyscallArg(tc, index);

#if THE_ISA == RISCV_ISA or THE_ISA == ARM_ISA
    /**
     * Linux sets CLONE_BACKWARDS flag for RISC-V and Arm.
     * The flag defines the list of clone() arguments in the following
     * order: flags -> newStack -> ptidPtr -> tlsPtr -> ctidPtr
     */
    Addr tlsPtr = p->getSyscallArg(tc, index);
    Addr ctidPtr = p->getSyscallArg(tc, index);
#else
    Addr ctidPtr = p->getSyscallArg(tc, index);
    Addr tlsPtr = p->getSyscallArg(tc, index);
#endif

    if (((flags & OS::TGT_CLONE_SIGHAND)&& !(flags & OS::TGT_CLONE_VM)) ||
        ((flags & OS::TGT_CLONE_THREAD) && !(flags & OS::TGT_CLONE_SIGHAND)) ||
        ((flags & OS::TGT_CLONE_FS)     &&  (flags & OS::TGT_CLONE_NEWNS)) ||
        ((flags & OS::TGT_CLONE_NEWIPC) &&  (flags & OS::TGT_CLONE_SYSVSEM)) ||
        ((flags & OS::TGT_CLONE_NEWPID) &&  (flags & OS::TGT_CLONE_THREAD)) ||
        ((flags & OS::TGT_CLONE_VM)     && !(newStack)))
        return -EINVAL;

    ThreadContext *ctc;
    if (!(ctc = p->findFreeContext())) {
        DPRINTF_SYSCALL(Verbose, "clone: no spare thread context in system"
                        "[cpu %d, thread %d]", tc->cpuId(), tc->threadId());
        return -EAGAIN;
    }

    /**
     * Note that ProcessParams is generated by swig and there are no other
     * examples of how to create anything but this default constructor. The
     * fields are manually initialized instead of passing parameters to the
     * constructor.
     */
    ProcessParams *pp = new ProcessParams();
    pp->executable.assign(*(new std::string(p->progName())));
    pp->cmd.push_back(*(new std::string(p->progName())));
    pp->system = p->system;
    pp->cwd.assign(p->getcwd());
    pp->input.assign("stdin");
    pp->output.assign("stdout");
    pp->errout.assign("stderr");
    pp->uid = p->uid();
    pp->euid = p->euid();
    pp->gid = p->gid();
    pp->egid = p->egid();

    /* Find the first free PID that's less than the maximum */
    std::set<int> const& pids = p->system->PIDs;
    int temp_pid = *pids.begin();
    do {
        temp_pid++;
    } while (pids.find(temp_pid) != pids.end());
    if (temp_pid >= System::maxPID)
        fatal("temp_pid is too large: %d", temp_pid);

    pp->pid = temp_pid;
    pp->ppid = (flags & OS::TGT_CLONE_THREAD) ? p->ppid() : p->pid();
    Process *cp = pp->create();
    delete pp;

    Process *owner = ctc->getProcessPtr();
    ctc->setProcessPtr(cp);
    cp->assignThreadContext(ctc->contextId());
    owner->revokeThreadContext(ctc->contextId());

    ctc->clearArchRegs();

    if (flags & OS::TGT_CLONE_PARENT_SETTID) {
        BufferArg ptidBuf(ptidPtr, sizeof(long));
        long *ptid = (long *)ptidBuf.bufferPtr();
        *ptid = cp->pid();
        ptidBuf.copyOut(tc->getMemProxy());
    }

    // We don't need to allocate space for code and data segments for this new
    // thread since it's gonna share them with its parent.
    if (flags & OS::TGT_CLONE_VM)
        cp->needToInitArgs = false;

    cp->initState();
    p->clone(tc, ctc, cp, flags);

    if (flags & OS::TGT_CLONE_THREAD) {
        delete cp->sigchld;
        cp->sigchld = p->sigchld;
    } else if (flags & OS::TGT_SIGCHLD) {
        *cp->sigchld = true;
    }

    if (flags & OS::TGT_CLONE_CHILD_SETTID) {
        BufferArg ctidBuf(ctidPtr, sizeof(long));
        long *ctid = (long *)ctidBuf.bufferPtr();
        *ctid = cp->pid();
        ctidBuf.copyOut(ctc->getMemProxy());
    }

    if (flags & OS::TGT_CLONE_CHILD_CLEARTID)
        cp->childClearTID = (uint64_t)ctidPtr;

    OS::archClone(flags, p, cp, tc, ctc, newStack, tlsPtr);

    cp->setSyscallReturn(ctc, 0);

#if THE_ISA == ALPHA_ISA
    ctc->setIntReg(TheISA::SyscallSuccessReg, 0);
#elif THE_ISA == SPARC_ISA
    tc->setIntReg(TheISA::SyscallPseudoReturnReg, 0);
    ctc->setIntReg(TheISA::SyscallPseudoReturnReg, 1);
#endif

    TheISA::PCState cpc = tc->pcState();
    cpc.advance();
    ctc->pcState(cpc);

    // when a process is initialized, a thread context may have been already
    // activated, so we don't want to activate it twice.
    if (ctc->status() != ThreadContext::Active)
        ctc->activate();

    return cp->pid();
}

/// Target fstatfs() handler.
template <class OS>
SyscallReturn
fstatfsFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    Addr bufPtr = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    struct statfs hostBuf;
    int result = fstatfs(sim_fd, &hostBuf);

    if (result < 0)
        return -errno;

    copyOutStatfsBuf<OS>(tc->getMemProxy(), bufPtr, &hostBuf);

    return 0;
}

/// Target readv() handler.
template <class OS>
SyscallReturn
readvFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    SETranslatingPortProxy &prox = tc->getMemProxy();
    uint64_t tiov_base = p->getSyscallArg(tc, index);
    size_t count = p->getSyscallArg(tc, index);
    typename OS::tgt_iovec tiov[count];
    struct iovec hiov[count];
    for (size_t i = 0; i < count; ++i) {
        prox.readBlob(tiov_base + (i * sizeof(typename OS::tgt_iovec)),
                      (uint8_t*)&tiov[i], sizeof(typename OS::tgt_iovec));
        hiov[i].iov_len = TheISA::gtoh(tiov[i].iov_len);
        hiov[i].iov_base = new char [hiov[i].iov_len];
    }

    int result = readv(sim_fd, hiov, count);
    int local_errno = errno;

    for (size_t i = 0; i < count; ++i) {
        if (result != -1) {
            prox.writeBlob(TheISA::htog(tiov[i].iov_base),
                           (uint8_t*)hiov[i].iov_base, hiov[i].iov_len);
        }
        delete [] (char *)hiov[i].iov_base;
    }

    return (result == -1) ? -local_errno : result;
}

/// Target writev() handler.
template <class OS>
SyscallReturn
writevFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);

    auto hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[tgt_fd]);
    if (!hbfdp)
        return -EBADF;
    int sim_fd = hbfdp->getSimFD();

    SETranslatingPortProxy &prox = tc->getMemProxy();
    uint64_t tiov_base = p->getSyscallArg(tc, index);
    size_t count = p->getSyscallArg(tc, index);
    struct iovec hiov[count];
    for (size_t i = 0; i < count; ++i) {
        typename OS::tgt_iovec tiov;

        prox.readBlob(tiov_base + i*sizeof(typename OS::tgt_iovec),
                      (uint8_t*)&tiov, sizeof(typename OS::tgt_iovec));
        hiov[i].iov_len = TheISA::gtoh(tiov.iov_len);
        hiov[i].iov_base = new char [hiov[i].iov_len];
        prox.readBlob(TheISA::gtoh(tiov.iov_base), (uint8_t *)hiov[i].iov_base,
                      hiov[i].iov_len);
    }

    int result = writev(sim_fd, hiov, count);

    for (size_t i = 0; i < count; ++i)
        delete [] (char *)hiov[i].iov_base;

    return (result == -1) ? -errno : result;
}

/// Real mmap handler.
template <class OS>
SyscallReturn
mmapImpl(SyscallDesc *desc, int num, Process *p, ThreadContext *tc,
         bool is_mmap2)
{
    int index = 0;
    Addr start = p->getSyscallArg(tc, index);
    uint64_t length = p->getSyscallArg(tc, index);
    int prot = p->getSyscallArg(tc, index);
    int tgt_flags = p->getSyscallArg(tc, index);
    int tgt_fd = p->getSyscallArg(tc, index);
    int offset = p->getSyscallArg(tc, index);

    if (is_mmap2)
        offset *= TheISA::PageBytes;

    if (start & (TheISA::PageBytes - 1) ||
        offset & (TheISA::PageBytes - 1) ||
        (tgt_flags & OS::TGT_MAP_PRIVATE &&
         tgt_flags & OS::TGT_MAP_SHARED) ||
        (!(tgt_flags & OS::TGT_MAP_PRIVATE) &&
         !(tgt_flags & OS::TGT_MAP_SHARED)) ||
        !length) {
        return -EINVAL;
    }

    if ((prot & PROT_WRITE) && (tgt_flags & OS::TGT_MAP_SHARED)) {
        // With shared mmaps, there are two cases to consider:
        // 1) anonymous: writes should modify the mapping and this should be
        // visible to observers who share the mapping. Currently, it's
        // difficult to update the shared mapping because there's no
        // structure which maintains information about the which virtual
        // memory areas are shared. If that structure existed, it would be
        // possible to make the translations point to the same frames.
        // 2) file-backed: writes should modify the mapping and the file
        // which is backed by the mapping. The shared mapping problem is the
        // same as what was mentioned about the anonymous mappings. For
        // file-backed mappings, the writes to the file are difficult
        // because it requires syncing what the mapping holds with the file
        // that resides on the host system. So, any write on a real system
        // would cause the change to be propagated to the file mapping at
        // some point in the future (the inode is tracked along with the
        // mapping). This isn't guaranteed to always happen, but it usually
        // works well enough. The guarantee is provided by the msync system
        // call. We could force the change through with shared mappings with
        // a call to msync, but that again would require more information
        // than we currently maintain.
        warn("mmap: writing to shared mmap region is currently "
             "unsupported. The write succeeds on the target, but it "
             "will not be propagated to the host or shared mappings");
    }

    length = roundUp(length, TheISA::PageBytes);

    int sim_fd = -1;
    uint8_t *pmap = nullptr;
    if (!(tgt_flags & OS::TGT_MAP_ANONYMOUS)) {
        std::shared_ptr<FDEntry> fdep = (*p->fds)[tgt_fd];

        auto dfdp = std::dynamic_pointer_cast<DeviceFDEntry>(fdep);
        if (dfdp) {
            EmulatedDriver *emul_driver = dfdp->getDriver();
            return emul_driver->mmap(p, tc, start, length, prot,
                                     tgt_flags, tgt_fd, offset);
        }

        auto ffdp = std::dynamic_pointer_cast<FileFDEntry>(fdep);
        if (!ffdp)
            return -EBADF;
        sim_fd = ffdp->getSimFD();

        pmap = (decltype(pmap))mmap(nullptr, length, PROT_READ, MAP_PRIVATE,
                                    sim_fd, offset);

        if (pmap == (decltype(pmap))-1) {
            warn("mmap: failed to map file into host address space");
            return -errno;
        }
    }

    // Extend global mmap region if necessary. Note that we ignore the
    // start address unless MAP_FIXED is specified.
    if (!(tgt_flags & OS::TGT_MAP_FIXED)) {
        std::shared_ptr<MemState> mem_state = p->memState;
        Addr mmap_end = mem_state->getMmapEnd();

        start = p->mmapGrowsDown() ? mmap_end - length : mmap_end;
        mmap_end = p->mmapGrowsDown() ? start : mmap_end + length;

        mem_state->setMmapEnd(mmap_end);
    }

    DPRINTF_SYSCALL(Verbose, " mmap range is 0x%x - 0x%x\n",
                    start, start + length - 1);

    // We only allow mappings to overwrite existing mappings if
    // TGT_MAP_FIXED is set. Otherwise it shouldn't be a problem
    // because we ignore the start hint if TGT_MAP_FIXED is not set.
    int clobber = tgt_flags & OS::TGT_MAP_FIXED;
    if (clobber) {
        for (auto tc : p->system->threadContexts) {
            // If we might be overwriting old mappings, we need to
            // invalidate potentially stale mappings out of the TLBs.
            tc->getDTBPtr()->flushAll();
            tc->getITBPtr()->flushAll();
        }
    }

    // Allocate physical memory and map it in. If the page table is already
    // mapped and clobber is not set, the simulator will issue throw a
    // fatal and bail out of the simulation.
    p->allocateMemThreadCtx(start, length, clobber, tc);

    // Transfer content into target address space.
    SETranslatingPortProxy &tp = tc->getMemProxy();
    if (tgt_flags & OS::TGT_MAP_ANONYMOUS) {
        // In general, we should zero the mapped area for anonymous mappings,
        // with something like:
        //     tp.memsetBlob(start, 0, length);
        // However, given that we don't support sparse mappings, and
        // some applications can map a couple of gigabytes of space
        // (intending sparse usage), that can get painfully expensive.
        // Fortunately, since we don't properly implement munmap either,
        // there's no danger of remapping used memory, so for now all
        // newly mapped memory should already be zeroed so we can skip it.
    } else {
        // It is possible to mmap an area larger than a file, however
        // accessing unmapped portions the system triggers a "Bus error"
        // on the host. We must know when to stop copying the file from
        // the host into the target address space.
        struct stat file_stat;
        if (fstat(sim_fd, &file_stat) > 0)
            fatal("mmap: cannot stat file");

        // Copy the portion of the file that is resident. This requires
        // checking both the mmap size and the filesize that we are
        // trying to mmap into this space; the mmap size also depends
        // on the specified offset into the file.
        uint64_t size = std::min((uint64_t)file_stat.st_size - offset,
                                 length);
        tp.writeBlob(start, pmap, size);

        // Cleanup the mmap region before exiting this function.
        munmap(pmap, length);

        // Maintain the symbol table for dynamic executables.
        // The loader will call mmap to map the images into its address
        // space and we intercept that here. We can verify that we are
        // executing inside the loader by checking the program counter value.
        // XXX: with multiprogrammed workloads or multi-node configurations,
        // this will not work since there is a single global symbol table.
        ObjectFile *interpreter = p->getInterpreter();
        if (interpreter) {
            Addr text_start = interpreter->textBase();
            Addr text_end = text_start + interpreter->textSize();

            Addr pc = tc->pcState().pc();

            if (pc >= text_start && pc < text_end) {
                std::shared_ptr<FDEntry> fdep = (*p->fds)[tgt_fd];
                auto ffdp = std::dynamic_pointer_cast<FileFDEntry>(fdep);
                ObjectFile *lib = createObjectFile(ffdp->getFileName());

                if (lib) {
                    lib->loadAllSymbols(debugSymbolTable,
                                        lib->textBase(), start);
                }
            }
        }

        // Note that we do not zero out the remainder of the mapping. This
        // is done by a real system, but it probably will not affect
        // execution (hopefully).
    }

    return start;
}

template <class OS>
SyscallReturn
pwrite64Func(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    Addr bufPtr = p->getSyscallArg(tc, index);
    int nbytes = p->getSyscallArg(tc, index);
    int offset = p->getSyscallArg(tc, index);

    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>((*p->fds)[tgt_fd]);
    if (!ffdp)
        return -EBADF;
    int sim_fd = ffdp->getSimFD();

    BufferArg bufArg(bufPtr, nbytes);
    bufArg.copyIn(tc->getMemProxy());

    int bytes_written = pwrite(sim_fd, bufArg.bufferPtr(), nbytes, offset);

    return (bytes_written == -1) ? -errno : bytes_written;
}

/// Target mmap() handler.
template <class OS>
SyscallReturn
mmapFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    return mmapImpl<OS>(desc, num, p, tc, false);
}

/// Target mmap2() handler.
template <class OS>
SyscallReturn
mmap2Func(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    return mmapImpl<OS>(desc, num, p, tc, true);
}

/// Target getrlimit() handler.
template <class OS>
SyscallReturn
getrlimitFunc(SyscallDesc *desc, int callnum, Process *process,
              ThreadContext *tc)
{
    int index = 0;
    unsigned resource = process->getSyscallArg(tc, index);
    TypedBufferArg<typename OS::rlimit> rlp(process->getSyscallArg(tc, index));

    switch (resource) {
      case OS::TGT_RLIMIT_STACK:
        // max stack size in bytes: make up a number (8MB for now)
        rlp->rlim_cur = rlp->rlim_max = 8 * 1024 * 1024;
        rlp->rlim_cur = TheISA::htog(rlp->rlim_cur);
        rlp->rlim_max = TheISA::htog(rlp->rlim_max);
        break;

      case OS::TGT_RLIMIT_DATA:
        // max data segment size in bytes: make up a number
        rlp->rlim_cur = rlp->rlim_max = 256 * 1024 * 1024;
        rlp->rlim_cur = TheISA::htog(rlp->rlim_cur);
        rlp->rlim_max = TheISA::htog(rlp->rlim_max);
        break;

      default:
        warn("getrlimit: unimplemented resource %d", resource);
        return -EINVAL;
        break;
    }

    rlp.copyOut(tc->getMemProxy());
    return 0;
}

template <class OS>
SyscallReturn
prlimitFunc(SyscallDesc *desc, int callnum, Process *process,
            ThreadContext *tc)
{
    int index = 0;
    if (process->getSyscallArg(tc, index) != 0)
    {
        warn("prlimit: ignoring rlimits for nonzero pid");
        return -EPERM;
    }
    int resource = process->getSyscallArg(tc, index);
    Addr n = process->getSyscallArg(tc, index);
    if (n != 0)
        warn("prlimit: ignoring new rlimit");
    Addr o = process->getSyscallArg(tc, index);
    if (o != 0)
    {
        TypedBufferArg<typename OS::rlimit> rlp(o);
        switch (resource) {
          case OS::TGT_RLIMIT_STACK:
            // max stack size in bytes: make up a number (8MB for now)
            rlp->rlim_cur = rlp->rlim_max = 8 * 1024 * 1024;
            rlp->rlim_cur = TheISA::htog(rlp->rlim_cur);
            rlp->rlim_max = TheISA::htog(rlp->rlim_max);
            break;
          case OS::TGT_RLIMIT_DATA:
            // max data segment size in bytes: make up a number
            rlp->rlim_cur = rlp->rlim_max = 256*1024*1024;
            rlp->rlim_cur = TheISA::htog(rlp->rlim_cur);
            rlp->rlim_max = TheISA::htog(rlp->rlim_max);
            break;
          default:
            warn("prlimit: unimplemented resource %d", resource);
            return -EINVAL;
            break;
        }
        rlp.copyOut(tc->getMemProxy());
    }
    return 0;
}

/// Target clock_gettime() function.
template <class OS>
SyscallReturn
clock_gettimeFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 1;
    //int clk_id = p->getSyscallArg(tc, index);
    TypedBufferArg<typename OS::timespec> tp(p->getSyscallArg(tc, index));

    getElapsedTimeNano(tp->tv_sec, tp->tv_nsec);
    tp->tv_sec += seconds_since_epoch;
    tp->tv_sec = TheISA::htog(tp->tv_sec);
    tp->tv_nsec = TheISA::htog(tp->tv_nsec);

    tp.copyOut(tc->getMemProxy());

    return 0;
}

/// Target clock_getres() function.
template <class OS>
SyscallReturn
clock_getresFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 1;
    TypedBufferArg<typename OS::timespec> tp(p->getSyscallArg(tc, index));

    // Set resolution at ns, which is what clock_gettime() returns
    tp->tv_sec = 0;
    tp->tv_nsec = 1;

    tp.copyOut(tc->getMemProxy());

    return 0;
}

/// Target gettimeofday() handler.
template <class OS>
SyscallReturn
gettimeofdayFunc(SyscallDesc *desc, int callnum, Process *process,
                 ThreadContext *tc)
{
    int index = 0;
    TypedBufferArg<typename OS::timeval> tp(process->getSyscallArg(tc, index));

    getElapsedTimeMicro(tp->tv_sec, tp->tv_usec);
    tp->tv_sec += seconds_since_epoch;
    tp->tv_sec = TheISA::htog(tp->tv_sec);
    tp->tv_usec = TheISA::htog(tp->tv_usec);

    tp.copyOut(tc->getMemProxy());

    return 0;
}


/// Target utimes() handler.
template <class OS>
SyscallReturn
utimesFunc(SyscallDesc *desc, int callnum, Process *process,
           ThreadContext *tc)
{
    std::string path;

    int index = 0;
    if (!tc->getMemProxy().tryReadString(path,
                process->getSyscallArg(tc, index))) {
        return -EFAULT;
    }

    TypedBufferArg<typename OS::timeval [2]>
        tp(process->getSyscallArg(tc, index));
    tp.copyIn(tc->getMemProxy());

    struct timeval hostTimeval[2];
    for (int i = 0; i < 2; ++i) {
        hostTimeval[i].tv_sec = TheISA::gtoh((*tp)[i].tv_sec);
        hostTimeval[i].tv_usec = TheISA::gtoh((*tp)[i].tv_usec);
    }

    // Adjust path for current working directory
    path = process->fullPath(path);

    int result = utimes(path.c_str(), hostTimeval);

    if (result < 0)
        return -errno;

    return 0;
}

template <class OS>
SyscallReturn
execveFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    desc->setFlags(0);

    int index = 0;
    std::string path;
    SETranslatingPortProxy & mem_proxy = tc->getMemProxy();
    if (!mem_proxy.tryReadString(path, p->getSyscallArg(tc, index)))
        return -EFAULT;

    if (access(path.c_str(), F_OK) == -1)
        return -EACCES;

    auto read_in = [](std::vector<std::string> & vect,
                      SETranslatingPortProxy & mem_proxy,
                      Addr mem_loc)
    {
        for (int inc = 0; ; inc++) {
            BufferArg b((mem_loc + sizeof(Addr) * inc), sizeof(Addr));
            b.copyIn(mem_proxy);

            if (!*(Addr*)b.bufferPtr())
                break;

            vect.push_back(std::string());
            mem_proxy.tryReadString(vect[inc], *(Addr*)b.bufferPtr());
        }
    };

    /**
     * Note that ProcessParams is generated by swig and there are no other
     * examples of how to create anything but this default constructor. The
     * fields are manually initialized instead of passing parameters to the
     * constructor.
     */
    ProcessParams *pp = new ProcessParams();
    pp->executable = path;
    Addr argv_mem_loc = p->getSyscallArg(tc, index);
    read_in(pp->cmd, mem_proxy, argv_mem_loc);
    Addr envp_mem_loc = p->getSyscallArg(tc, index);
    read_in(pp->env, mem_proxy, envp_mem_loc);
    pp->uid = p->uid();
    pp->egid = p->egid();
    pp->euid = p->euid();
    pp->gid = p->gid();
    pp->ppid = p->ppid();
    pp->pid = p->pid();
    pp->input.assign("cin");
    pp->output.assign("cout");
    pp->errout.assign("cerr");
    pp->cwd.assign(p->getcwd());
    pp->system = p->system;
    /**
     * Prevent process object creation with identical PIDs (which will trip
     * a fatal check in Process constructor). The execve call is supposed to
     * take over the currently executing process' identity but replace
     * whatever it is doing with a new process image. Instead of hijacking
     * the process object in the simulator, we create a new process object
     * and bind to the previous process' thread below (hijacking the thread).
     */
    p->system->PIDs.erase(p->pid());
    Process *new_p = pp->create();
    delete pp;

    /**
     * Work through the file descriptor array and close any files marked
     * close-on-exec.
     */
    new_p->fds = p->fds;
    for (int i = 0; i < new_p->fds->getSize(); i++) {
        std::shared_ptr<FDEntry> fdep = (*new_p->fds)[i];
        if (fdep && fdep->getCOE())
            new_p->fds->closeFDEntry(i);
    }

    *new_p->sigchld = true;

    delete p;
    tc->clearArchRegs();
    tc->setProcessPtr(new_p);
    new_p->assignThreadContext(tc->contextId());
    new_p->initState();
    tc->activate();
    TheISA::PCState pcState = tc->pcState();
    tc->setNPC(pcState.instAddr());

    desc->setFlags(SyscallDesc::SuppressReturnValue);
    return 0;
}

/// Target getrusage() function.
template <class OS>
SyscallReturn
getrusageFunc(SyscallDesc *desc, int callnum, Process *process,
              ThreadContext *tc)
{
    int index = 0;
    int who = process->getSyscallArg(tc, index); // THREAD, SELF, or CHILDREN
    TypedBufferArg<typename OS::rusage> rup(process->getSyscallArg(tc, index));

    rup->ru_utime.tv_sec = 0;
    rup->ru_utime.tv_usec = 0;
    rup->ru_stime.tv_sec = 0;
    rup->ru_stime.tv_usec = 0;
    rup->ru_maxrss = 0;
    rup->ru_ixrss = 0;
    rup->ru_idrss = 0;
    rup->ru_isrss = 0;
    rup->ru_minflt = 0;
    rup->ru_majflt = 0;
    rup->ru_nswap = 0;
    rup->ru_inblock = 0;
    rup->ru_oublock = 0;
    rup->ru_msgsnd = 0;
    rup->ru_msgrcv = 0;
    rup->ru_nsignals = 0;
    rup->ru_nvcsw = 0;
    rup->ru_nivcsw = 0;

    switch (who) {
      case OS::TGT_RUSAGE_SELF:
        getElapsedTimeMicro(rup->ru_utime.tv_sec, rup->ru_utime.tv_usec);
        rup->ru_utime.tv_sec = TheISA::htog(rup->ru_utime.tv_sec);
        rup->ru_utime.tv_usec = TheISA::htog(rup->ru_utime.tv_usec);
        break;

      case OS::TGT_RUSAGE_CHILDREN:
        // do nothing.  We have no child processes, so they take no time.
        break;

      default:
        // don't really handle THREAD or CHILDREN, but just warn and
        // plow ahead
        warn("getrusage() only supports RUSAGE_SELF.  Parameter %d ignored.",
             who);
    }

    rup.copyOut(tc->getMemProxy());

    return 0;
}

/// Target times() function.
template <class OS>
SyscallReturn
timesFunc(SyscallDesc *desc, int callnum, Process *process,
          ThreadContext *tc)
{
    int index = 0;
    TypedBufferArg<typename OS::tms> bufp(process->getSyscallArg(tc, index));

    // Fill in the time structure (in clocks)
    int64_t clocks = curTick() * OS::M5_SC_CLK_TCK / SimClock::Int::s;
    bufp->tms_utime = clocks;
    bufp->tms_stime = 0;
    bufp->tms_cutime = 0;
    bufp->tms_cstime = 0;

    // Convert to host endianness
    bufp->tms_utime = TheISA::htog(bufp->tms_utime);

    // Write back
    bufp.copyOut(tc->getMemProxy());

    // Return clock ticks since system boot
    return clocks;
}

/// Target time() function.
template <class OS>
SyscallReturn
timeFunc(SyscallDesc *desc, int callnum, Process *process, ThreadContext *tc)
{
    typename OS::time_t sec, usec;
    getElapsedTimeMicro(sec, usec);
    sec += seconds_since_epoch;

    int index = 0;
    Addr taddr = (Addr)process->getSyscallArg(tc, index);
    if (taddr != 0) {
        typename OS::time_t t = sec;
        t = TheISA::htog(t);
        SETranslatingPortProxy &p = tc->getMemProxy();
        p.writeBlob(taddr, (uint8_t*)&t, (int)sizeof(typename OS::time_t));
    }
    return sec;
}

template <class OS>
SyscallReturn
tgkillFunc(SyscallDesc *desc, int num, Process *process, ThreadContext *tc)
{
    int index = 0;
    int tgid = process->getSyscallArg(tc, index);
    int tid = process->getSyscallArg(tc, index);
    int sig = process->getSyscallArg(tc, index);

    /**
     * This system call is intended to allow killing a specific thread
     * within an arbitrary thread group if sanctioned with permission checks.
     * It's usually true that threads share the termination signal as pointed
     * out by the pthread_kill man page and this seems to be the intended
     * usage. Due to this being an emulated environment, assume the following:
     * Threads are allowed to call tgkill because the EUID for all threads
     * should be the same. There is no signal handling mechanism for kernel
     * registration of signal handlers since signals are poorly supported in
     * emulation mode. Since signal handlers cannot be registered, all
     * threads within in a thread group must share the termination signal.
     * We never exhaust PIDs so there's no chance of finding the wrong one
     * due to PID rollover.
     */

    System *sys = tc->getSystemPtr();
    Process *tgt_proc = nullptr;
    for (int i = 0; i < sys->numContexts(); i++) {
        Process *temp = sys->threadContexts[i]->getProcessPtr();
        if (temp->pid() == tid) {
            tgt_proc = temp;
            break;
        }
    }

    if (sig != 0 || sig != OS::TGT_SIGABRT)
        return -EINVAL;

    if (tgt_proc == nullptr)
        return -ESRCH;

    if (tgid != -1 && tgt_proc->tgid() != tgid)
        return -ESRCH;

    if (sig == OS::TGT_SIGABRT)
        exitGroupFunc(desc, 252, process, tc);

    return 0;
}

template <class OS>
SyscallReturn
socketFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int domain = p->getSyscallArg(tc, index);
    int type = p->getSyscallArg(tc, index);
    int prot = p->getSyscallArg(tc, index);

    int sim_fd = socket(domain, type, prot);
    if (sim_fd == -1)
        return -errno;

    auto sfdp = std::make_shared<SocketFDEntry>(sim_fd, domain, type, prot);
    int tgt_fd = p->fds->allocFD(sfdp);

    return tgt_fd;
}

template <class OS>
SyscallReturn
socketpairFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int domain = p->getSyscallArg(tc, index);
    int type = p->getSyscallArg(tc, index);
    int prot = p->getSyscallArg(tc, index);
    Addr svPtr = p->getSyscallArg(tc, index);

    BufferArg svBuf((Addr)svPtr, 2 * sizeof(int));
    int status = socketpair(domain, type, prot, (int *)svBuf.bufferPtr());
    if (status == -1)
        return -errno;

    int *fds = (int *)svBuf.bufferPtr();

    auto sfdp1 = std::make_shared<SocketFDEntry>(fds[0], domain, type, prot);
    fds[0] = p->fds->allocFD(sfdp1);
    auto sfdp2 = std::make_shared<SocketFDEntry>(fds[1], domain, type, prot);
    fds[1] = p->fds->allocFD(sfdp2);
    svBuf.copyOut(tc->getMemProxy());

    return status;
}

template <class OS>
SyscallReturn
selectFunc(SyscallDesc *desc, int callnum, Process *p, ThreadContext *tc)
{
    int retval;

    int index = 0;
    int nfds_t = p->getSyscallArg(tc, index);
    Addr fds_read_ptr = p->getSyscallArg(tc, index);
    Addr fds_writ_ptr = p->getSyscallArg(tc, index);
    Addr fds_excp_ptr = p->getSyscallArg(tc, index);
    Addr time_val_ptr = p->getSyscallArg(tc, index);

    TypedBufferArg<typename OS::fd_set> rd_t(fds_read_ptr);
    TypedBufferArg<typename OS::fd_set> wr_t(fds_writ_ptr);
    TypedBufferArg<typename OS::fd_set> ex_t(fds_excp_ptr);
    TypedBufferArg<typename OS::timeval> tp(time_val_ptr);

    /**
     * Host fields. Notice that these use the definitions from the system
     * headers instead of the gem5 headers and libraries. If the host and
     * target have different header file definitions, this will not work.
     */
    fd_set rd_h;
    FD_ZERO(&rd_h);
    fd_set wr_h;
    FD_ZERO(&wr_h);
    fd_set ex_h;
    FD_ZERO(&ex_h);

    /**
     * Copy in the fd_set from the target.
     */
    if (fds_read_ptr)
        rd_t.copyIn(tc->getMemProxy());
    if (fds_writ_ptr)
        wr_t.copyIn(tc->getMemProxy());
    if (fds_excp_ptr)
        ex_t.copyIn(tc->getMemProxy());

    /**
     * We need to translate the target file descriptor set into a host file
     * descriptor set. This involves both our internal process fd array
     * and the fd_set defined in Linux header files. The nfds field also
     * needs to be updated as it will be only target specific after
     * retrieving it from the target; the nfds value is expected to be the
     * highest file descriptor that needs to be checked, so we need to extend
     * it out for nfds_h when we do the update.
     */
    int nfds_h = 0;
    std::map<int, int> trans_map;
    auto try_add_host_set = [&](fd_set *tgt_set_entry,
                                fd_set *hst_set_entry,
                                int iter) -> bool
    {
        /**
         * By this point, we know that we are looking at a valid file
         * descriptor set on the target. We need to check if the target file
         * descriptor value passed in as iter is part of the set.
         */
        if (FD_ISSET(iter, tgt_set_entry)) {
            /**
             * We know that the target file descriptor belongs to the set,
             * but we do not yet know if the file descriptor is valid or
             * that we have a host mapping. Check that now.
             */
            auto hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[iter]);
            if (!hbfdp)
                return true;
            auto sim_fd = hbfdp->getSimFD();

            /**
             * Add the sim_fd to tgt_fd translation into trans_map for use
             * later when we need to zero the target fd_set structures and
             * then update them with hits returned from the host select call.
             */
            trans_map[sim_fd] = iter;

            /**
             * We know that the host file descriptor exists so now we check
             * if we need to update the max count for nfds_h before passing
             * the duplicated structure into the host.
             */
            nfds_h = std::max(nfds_h - 1, sim_fd + 1);

            /**
             * Add the host file descriptor to the set that we are going to
             * pass into the host.
             */
            FD_SET(sim_fd, hst_set_entry);
        }
        return false;
    };

    for (int i = 0; i < nfds_t; i++) {
        if (fds_read_ptr) {
            bool ebadf = try_add_host_set((fd_set*)&*rd_t, &rd_h, i);
            if (ebadf) return -EBADF;
        }
        if (fds_writ_ptr) {
            bool ebadf = try_add_host_set((fd_set*)&*wr_t, &wr_h, i);
            if (ebadf) return -EBADF;
        }
        if (fds_excp_ptr) {
            bool ebadf = try_add_host_set((fd_set*)&*ex_t, &ex_h, i);
            if (ebadf) return -EBADF;
        }
    }

    if (time_val_ptr) {
        /**
         * It might be possible to decrement the timeval based on some
         * derivation of wall clock determined from elapsed simulator ticks
         * but that seems like overkill. Rather, we just set the timeval with
         * zero timeout. (There is no reason to block during the simulation
         * as it only decreases simulator performance.)
         */
        tp->tv_sec = 0;
        tp->tv_usec = 0;

        retval = select(nfds_h,
                        fds_read_ptr ? &rd_h : nullptr,
                        fds_writ_ptr ? &wr_h : nullptr,
                        fds_excp_ptr ? &ex_h : nullptr,
                        (timeval*)&*tp);
    } else {
        /**
         * If the timeval pointer is null, setup a new timeval structure to
         * pass into the host select call. Unfortunately, we will need to
         * manually check the return value and throw a retry fault if the
         * return value is zero. Allowing the system call to block will
         * likely deadlock the event queue.
         */
        struct timeval tv = { 0, 0 };

        retval = select(nfds_h,
                        fds_read_ptr ? &rd_h : nullptr,
                        fds_writ_ptr ? &wr_h : nullptr,
                        fds_excp_ptr ? &ex_h : nullptr,
                        &tv);

        if (retval == 0) {
            /**
             * If blocking indefinitely, check the signal list to see if a
             * signal would break the poll out of the retry cycle and try to
             * return the signal interrupt instead.
             */
            for (auto sig : tc->getSystemPtr()->signalList)
                if (sig.receiver == p)
                    return -EINTR;
            return SyscallReturn::retry();
        }
    }

    if (retval == -1)
        return -errno;

    FD_ZERO((fd_set*)&*rd_t);
    FD_ZERO((fd_set*)&*wr_t);
    FD_ZERO((fd_set*)&*ex_t);

    /**
     * We need to translate the host file descriptor set into a target file
     * descriptor set. This involves both our internal process fd array
     * and the fd_set defined in header files.
     */
    for (int i = 0; i < nfds_h; i++) {
        if (fds_read_ptr) {
            if (FD_ISSET(i, &rd_h))
                FD_SET(trans_map[i], (fd_set*)&*rd_t);
        }

        if (fds_writ_ptr) {
            if (FD_ISSET(i, &wr_h))
                FD_SET(trans_map[i], (fd_set*)&*wr_t);
        }

        if (fds_excp_ptr) {
            if (FD_ISSET(i, &ex_h))
                FD_SET(trans_map[i], (fd_set*)&*ex_t);
        }
    }

    if (fds_read_ptr)
        rd_t.copyOut(tc->getMemProxy());
    if (fds_writ_ptr)
        wr_t.copyOut(tc->getMemProxy());
    if (fds_excp_ptr)
        ex_t.copyOut(tc->getMemProxy());
    if (time_val_ptr)
        tp.copyOut(tc->getMemProxy());

    return retval;
}

template <class OS>
SyscallReturn
readFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    Addr buf_ptr = p->getSyscallArg(tc, index);
    int nbytes = p->getSyscallArg(tc, index);

    auto hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[tgt_fd]);
    if (!hbfdp)
        return -EBADF;
    int sim_fd = hbfdp->getSimFD();

    struct pollfd pfd;
    pfd.fd = sim_fd;
    pfd.events = POLLIN | POLLPRI;
    if ((poll(&pfd, 1, 0) == 0)
        && !(hbfdp->getFlags() & OS::TGT_O_NONBLOCK))
        return SyscallReturn::retry();

    BufferArg buf_arg(buf_ptr, nbytes);
    int bytes_read = read(sim_fd, buf_arg.bufferPtr(), nbytes);

    if (bytes_read > 0)
        buf_arg.copyOut(tc->getMemProxy());

    return (bytes_read == -1) ? -errno : bytes_read;
}

template <class OS>
SyscallReturn
writeFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    Addr buf_ptr = p->getSyscallArg(tc, index);
    int nbytes = p->getSyscallArg(tc, index);

    auto hbfdp = std::dynamic_pointer_cast<HBFDEntry>((*p->fds)[tgt_fd]);
    if (!hbfdp)
        return -EBADF;
    int sim_fd = hbfdp->getSimFD();

    BufferArg buf_arg(buf_ptr, nbytes);
    buf_arg.copyIn(tc->getMemProxy());

    struct pollfd pfd;
    pfd.fd = sim_fd;
    pfd.events = POLLOUT;

    /**
     * We don't want to poll on /dev/random. The kernel will not enable the
     * file descriptor for writing unless the entropy in the system falls
     * below write_wakeup_threshold. This is not guaranteed to happen
     * depending on host settings.
     */
    auto ffdp = std::dynamic_pointer_cast<FileFDEntry>(hbfdp);
    if (ffdp && (ffdp->getFileName() != "/dev/random")) {
        if (!poll(&pfd, 1, 0) && !(ffdp->getFlags() & OS::TGT_O_NONBLOCK))
            return SyscallReturn::retry();
    }

    int bytes_written = write(sim_fd, buf_arg.bufferPtr(), nbytes);

    if (bytes_written != -1)
        fsync(sim_fd);

    return (bytes_written == -1) ? -errno : bytes_written;
}

template <class OS>
SyscallReturn
wait4Func(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    int index = 0;
    pid_t pid = p->getSyscallArg(tc, index);
    Addr statPtr = p->getSyscallArg(tc, index);
    int options = p->getSyscallArg(tc, index);
    Addr rusagePtr = p->getSyscallArg(tc, index);

    if (rusagePtr)
        DPRINTFR(SyscallVerbose,
                 "%d: %s: syscall wait4: rusage pointer provided however "
                 "functionality not supported. Ignoring rusage pointer.\n",
                 curTick(), tc->getCpuPtr()->name());

    /**
     * Currently, wait4 is only implemented so that it will wait for children
     * exit conditions which are denoted by a SIGCHLD signals posted into the
     * system signal list. We return no additional information via any of the
     * parameters supplied to wait4. If nothing is found in the system signal
     * list, we will wait indefinitely for SIGCHLD to post by retrying the
     * call.
     */
    System *sysh = tc->getSystemPtr();
    std::list<BasicSignal>::iterator iter;
    for (iter=sysh->signalList.begin(); iter!=sysh->signalList.end(); iter++) {
        if (iter->receiver == p) {
            if (pid < -1) {
                if ((iter->sender->pgid() == -pid)
                    && (iter->signalValue == OS::TGT_SIGCHLD))
                    goto success;
            } else if (pid == -1) {
                if (iter->signalValue == OS::TGT_SIGCHLD)
                    goto success;
            } else if (pid == 0) {
                if ((iter->sender->pgid() == p->pgid())
                    && (iter->signalValue == OS::TGT_SIGCHLD))
                    goto success;
            } else {
                if ((iter->sender->pid() == pid)
                    && (iter->signalValue == OS::TGT_SIGCHLD))
                    goto success;
            }
        }
    }

    return (options & OS::TGT_WNOHANG) ? 0 : SyscallReturn::retry();

success:
    // Set status to EXITED for WIFEXITED evaluations.
    const int EXITED = 0;
    BufferArg statusBuf(statPtr, sizeof(int));
    *(int *)statusBuf.bufferPtr() = EXITED;
    statusBuf.copyOut(tc->getMemProxy());

    // Return the child PID.
    pid_t retval = iter->sender->pid();
    sysh->signalList.erase(iter);
    return retval;
}

template <class OS>
SyscallReturn
acceptFunc(SyscallDesc *desc, int num, Process *p, ThreadContext *tc)
{
    struct sockaddr sa;
    socklen_t addrLen;
    int host_fd;
    int index = 0;
    int tgt_fd = p->getSyscallArg(tc, index);
    Addr addrPtr = p->getSyscallArg(tc, index);
    Addr lenPtr = p->getSyscallArg(tc, index);

    BufferArg *lenBufPtr = nullptr;
    BufferArg *addrBufPtr = nullptr;

    auto sfdp = std::dynamic_pointer_cast<SocketFDEntry>((*p->fds)[tgt_fd]);
    if (!sfdp)
        return -EBADF;
    int sim_fd = sfdp->getSimFD();

    /**
     * We poll the socket file descriptor first to guarantee that we do not
     * block on our accept call. The socket can be opened without the
     * non-blocking flag (it blocks). This will cause deadlocks between
     * communicating processes.
     */
    struct pollfd pfd;
    pfd.fd = sim_fd;
    pfd.events = POLLIN | POLLPRI;
    if ((poll(&pfd, 1, 0) == 0)
        && !(sfdp->getFlags() & OS::TGT_O_NONBLOCK))
        return SyscallReturn::retry();

    if (lenPtr) {
        lenBufPtr = new BufferArg(lenPtr, sizeof(socklen_t));
        lenBufPtr->copyIn(tc->getMemProxy());
        memcpy(&addrLen, (socklen_t *)lenBufPtr->bufferPtr(),
               sizeof(socklen_t));
    }

    if (addrPtr) {
        addrBufPtr = new BufferArg(addrPtr, sizeof(struct sockaddr));
        addrBufPtr->copyIn(tc->getMemProxy());
        memcpy(&sa, (struct sockaddr *)addrBufPtr->bufferPtr(),
               sizeof(struct sockaddr));
    }

    host_fd = accept(sim_fd, &sa, &addrLen);

    if (host_fd == -1)
        return -errno;

    if (addrPtr) {
        memcpy(addrBufPtr->bufferPtr(), &sa, sizeof(sa));
        addrBufPtr->copyOut(tc->getMemProxy());
        delete(addrBufPtr);
    }

    if (lenPtr) {
        *(socklen_t *)lenBufPtr->bufferPtr() = addrLen;
        lenBufPtr->copyOut(tc->getMemProxy());
        delete(lenBufPtr);
    }

    auto afdp = std::make_shared<SocketFDEntry>(host_fd, sfdp->_domain,
                                                sfdp->_type, sfdp->_protocol);
    return p->fds->allocFD(afdp);
}

#endif // __SIM_SYSCALL_EMUL_HH__