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
 * Copyright (c) 2013 ARM Limited
 * Copyright (c) 2014-2015 Sven Karlsson
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
 * Copyright (c) 2016 RISC-V Foundation
 * Copyright (c) 2016 The University of Virginia
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
 * Authors: Andreas Hansson
 *          Sven Karlsson
 *          Alec Roelke
 */

#ifndef __ARCH_RISCV_REGISTERS_HH__
#define __ARCH_RISCV_REGISTERS_HH__

#include <map>
#include <string>
#include <vector>

#include "arch/generic/types.hh"
#include "arch/generic/vec_pred_reg.hh"
#include "arch/generic/vec_reg.hh"
#include "arch/isa_traits.hh"
#include "arch/riscv/generated/max_inst_regs.hh"
#include "base/types.hh"

namespace RiscvISA {

using RiscvISAInst::MaxInstSrcRegs;
using RiscvISAInst::MaxInstDestRegs;
const int MaxMiscDestRegs = 1;

// Not applicable to RISC-V
using VecElem = ::DummyVecElem;
using VecReg = ::DummyVecReg;
using ConstVecReg = ::DummyConstVecReg;
using VecRegContainer = ::DummyVecRegContainer;
constexpr unsigned NumVecElemPerVecReg = ::DummyNumVecElemPerVecReg;
constexpr size_t VecRegSizeBytes = ::DummyVecRegSizeBytes;

// Not applicable to RISC-V
using VecPredReg = ::DummyVecPredReg;
using ConstVecPredReg = ::DummyConstVecPredReg;
using VecPredRegContainer = ::DummyVecPredRegContainer;
constexpr size_t VecPredRegSizeBits = ::DummyVecPredRegSizeBits;
constexpr bool VecPredRegHasPackedRepr = ::DummyVecPredRegHasPackedRepr;

const int NumIntArchRegs = 32;
const int NumMicroIntRegs = 1;
const int NumIntRegs = NumIntArchRegs + NumMicroIntRegs;
const int NumFloatRegs = 32;

const unsigned NumVecRegs = 1;  // Not applicable to RISC-V
                                // (1 to prevent warnings)
const int NumVecPredRegs = 1;  // Not applicable to RISC-V
                               // (1 to prevent warnings)

const int NumCCRegs = 0;

// Semantically meaningful register indices
const int ZeroReg = 0;
const int ReturnAddrReg = 1;
const int StackPointerReg = 2;
const int GlobalPointerReg = 3;
const int ThreadPointerReg = 4;
const int FramePointerReg = 8;
const int ReturnValueReg = 10;
const std::vector<int> ReturnValueRegs = {10, 11};
const std::vector<int> ArgumentRegs = {10, 11, 12, 13, 14, 15, 16, 17};
const int AMOTempReg = 32;

const int SyscallPseudoReturnReg = 10;
const std::vector<int> SyscallArgumentRegs = {10, 11, 12, 13, 14, 15, 16};
const int SyscallNumReg = 17;

const std::vector<std::string> IntRegNames = {
    "zero", "ra", "sp", "gp",
    "tp", "t0", "t1", "t2",
    "s0", "s1", "a0", "a1",
    "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3",
    "s4", "s5", "s6", "s7",
    "s8", "s9", "s10", "s11",
    "t3", "t4", "t5", "t6"
};
const std::vector<std::string> FloatRegNames = {
    "ft0", "ft1", "ft2", "ft3",
    "ft4", "ft5", "ft6", "ft7",
    "fs0", "fs1", "fa0", "fa1",
    "fa2", "fa3", "fa4", "fa5",
    "fa6", "fa7", "fs2", "fs3",
    "fs4", "fs5", "fs6", "fs7",
    "fs8", "fs9", "fs10", "fs11",
    "ft8", "ft9", "ft10", "ft11"
};
const std::vector<std::string> RoccRegNames = {
    "z0",  "z1",  "z2",  "z3",
    "z4",  "z5",  "z6",  "z7",
    "z8",  "z9",  "z10", "z11",
    "z12", "z13", "z14", "z15",
    "z16", "z17", "z18", "z19",
    "z20", "z21", "z22", "z23",
    "z24", "z25", "z26", "z27",
    "z28", "z29", "z30", "z31"
};

const int NumToggleCSRs = 32;
const int NumValueCSRs = 32;

enum MiscRegIndex {
    MISCREG_PRV = 0,
    MISCREG_ISA,
    MISCREG_VENDORID,
    MISCREG_ARCHID,
    MISCREG_IMPID,
    MISCREG_HARTID,
    MISCREG_STATUS,
    MISCREG_IP,
    MISCREG_IE,
    MISCREG_CYCLE,
    MISCREG_TIME,
    MISCREG_INSTRET,
    MISCREG_HPMCOUNTER03,
    MISCREG_HPMCOUNTER04,
    MISCREG_HPMCOUNTER05,
    MISCREG_HPMCOUNTER06,
    MISCREG_HPMCOUNTER07,
    MISCREG_HPMCOUNTER08,
    MISCREG_HPMCOUNTER09,
    MISCREG_HPMCOUNTER10,
    MISCREG_HPMCOUNTER11,
    MISCREG_HPMCOUNTER12,
    MISCREG_HPMCOUNTER13,
    MISCREG_HPMCOUNTER14,
    MISCREG_HPMCOUNTER15,
    MISCREG_HPMCOUNTER16,
    MISCREG_HPMCOUNTER17,
    MISCREG_HPMCOUNTER18,
    MISCREG_HPMCOUNTER19,
    MISCREG_HPMCOUNTER20,
    MISCREG_HPMCOUNTER21,
    MISCREG_HPMCOUNTER22,
    MISCREG_HPMCOUNTER23,
    MISCREG_HPMCOUNTER24,
    MISCREG_HPMCOUNTER25,
    MISCREG_HPMCOUNTER26,
    MISCREG_HPMCOUNTER27,
    MISCREG_HPMCOUNTER28,
    MISCREG_HPMCOUNTER29,
    MISCREG_HPMCOUNTER30,
    MISCREG_HPMCOUNTER31,
    MISCREG_HPMEVENT03,
    MISCREG_HPMEVENT04,
    MISCREG_HPMEVENT05,
    MISCREG_HPMEVENT06,
    MISCREG_HPMEVENT07,
    MISCREG_HPMEVENT08,
    MISCREG_HPMEVENT09,
    MISCREG_HPMEVENT10,
    MISCREG_HPMEVENT11,
    MISCREG_HPMEVENT12,
    MISCREG_HPMEVENT13,
    MISCREG_HPMEVENT14,
    MISCREG_HPMEVENT15,
    MISCREG_HPMEVENT16,
    MISCREG_HPMEVENT17,
    MISCREG_HPMEVENT18,
    MISCREG_HPMEVENT19,
    MISCREG_HPMEVENT20,
    MISCREG_HPMEVENT21,
    MISCREG_HPMEVENT22,
    MISCREG_HPMEVENT23,
    MISCREG_HPMEVENT24,
    MISCREG_HPMEVENT25,
    MISCREG_HPMEVENT26,
    MISCREG_HPMEVENT27,
    MISCREG_HPMEVENT28,
    MISCREG_HPMEVENT29,
    MISCREG_HPMEVENT30,
    MISCREG_HPMEVENT31,
    MISCREG_TSELECT,
    MISCREG_TDATA1,
    MISCREG_TDATA2,
    MISCREG_TDATA3,
    MISCREG_DCSR,
    MISCREG_DPC,
    MISCREG_DSCRATCH,

    MISCREG_MEDELEG,
    MISCREG_MIDELEG,
    MISCREG_MTVEC,
    MISCREG_MCOUNTEREN,
    MISCREG_MSCRATCH,
    MISCREG_MEPC,
    MISCREG_MCAUSE,
    MISCREG_MTVAL,
    MISCREG_PMPCFG0,
    // pmpcfg1 rv32 only
    MISCREG_PMPCFG2,
    // pmpcfg3 rv32 only
    MISCREG_PMPADDR00,
    MISCREG_PMPADDR01,
    MISCREG_PMPADDR02,
    MISCREG_PMPADDR03,
    MISCREG_PMPADDR04,
    MISCREG_PMPADDR05,
    MISCREG_PMPADDR06,
    MISCREG_PMPADDR07,
    MISCREG_PMPADDR08,
    MISCREG_PMPADDR09,
    MISCREG_PMPADDR10,
    MISCREG_PMPADDR11,
    MISCREG_PMPADDR12,
    MISCREG_PMPADDR13,
    MISCREG_PMPADDR14,
    MISCREG_PMPADDR15,

    MISCREG_SEDELEG,
    MISCREG_SIDELEG,
    MISCREG_STVEC,
    MISCREG_SCOUNTEREN,
    MISCREG_SSCRATCH,
    MISCREG_SEPC,
    MISCREG_SCAUSE,
    MISCREG_STVAL,
    MISCREG_SATP,

    MISCREG_UTVEC,
    MISCREG_USCRATCH,
    MISCREG_UEPC,
    MISCREG_UCAUSE,
    MISCREG_UTVAL,
    MISCREG_FFLAGS,
    MISCREG_FRM,

    /*
     * Custom machine-level CSRs
     */

    // toggle simulation region tracking hardware stats
    // This CSR is used by an app to tell gem5 when to start tracking
    // performance statistics. Also, if --brg-fast-forward option is used,
    // toggling this CSR controls gem5 to transit between atomic and detailed
    // simulation modes.
    MISCREG_STATS_EN,

    MISCREG_STNT_BEGIN_VADDR,
    MISCREG_STNT_END_VADDR,
    MISCREG_STREL_BEGIN_VADDR,
    MISCREG_STREL_END_VADDR,
    MISCREG_BARRIER_INIT,
    MISCREG_BARRIER_WAIT,
    MISCREG_BARRIER_POLL,

    // control communication from an external manager to a processor. This
    // is read- and write-enabled.
    MISCREG_PROC2MNGR,

    // control communication from a processor to an external manager. This
    // is read-only.
    MISCREG_MNGR2PROC,

    // store the number of available hardware thread contexts.
    // This is read-only.
    MISCREG_NUMCORES,

    /*
     * Custom user-level toggle CSRs
     * There're 32 CSRs of this type. Each CSR which is 64-bit long can track
     * toggling 64 different events. Each event corresponds to a bit in each
     * CSR. The semantics of those CSRs are completely user-defined. gem5
     * only prints out when a bit in a CSR is toggled to facilitate BRG
     * tracing feature.
     */

    MISCREG_TOGGLE0,
    MISCREG_TOGGLE1,
    MISCREG_TOGGLE2,
    MISCREG_TOGGLE3,
    MISCREG_TOGGLE4,
    MISCREG_TOGGLE5,
    MISCREG_TOGGLE6,
    MISCREG_TOGGLE7,
    MISCREG_TOGGLE8,
    MISCREG_TOGGLE9,
    MISCREG_TOGGLE10,
    MISCREG_TOGGLE11,
    MISCREG_TOGGLE12,
    MISCREG_TOGGLE13,
    MISCREG_TOGGLE14,
    MISCREG_TOGGLE15,
    MISCREG_TOGGLE16,
    MISCREG_TOGGLE17,
    MISCREG_TOGGLE18,
    MISCREG_TOGGLE19,
    MISCREG_TOGGLE20,
    MISCREG_TOGGLE21,
    MISCREG_TOGGLE22,
    MISCREG_TOGGLE23,
    MISCREG_TOGGLE24,
    MISCREG_TOGGLE25,
    MISCREG_TOGGLE26,
    MISCREG_TOGGLE27,
    MISCREG_TOGGLE28,
    MISCREG_TOGGLE29,
    MISCREG_TOGGLE30,
    MISCREG_TOGGLE31,

    /*
     * Custom user-level value CSRs
     * There're 32 CSRs of this type. Each 64-bit CSR holds a user-defined
     * value that can accessed and updated by an application using CSR
     * instructions. gem5 outputs values of all value CSRs in its stats file.
     */

    MISCREG_VALUE0,
    MISCREG_VALUE1,
    MISCREG_VALUE2,
    MISCREG_VALUE3,
    MISCREG_VALUE4,
    MISCREG_VALUE5,
    MISCREG_VALUE6,
    MISCREG_VALUE7,
    MISCREG_VALUE8,
    MISCREG_VALUE9,
    MISCREG_VALUE10,
    MISCREG_VALUE11,
    MISCREG_VALUE12,
    MISCREG_VALUE13,
    MISCREG_VALUE14,
    MISCREG_VALUE15, // moyang TC3-TBIW: inv/wb type
    MISCREG_VALUE16, // moyang TC3-TBIW: inv/wb level
    MISCREG_VALUE17, // moyang TC3-TBIW: load/store level
    MISCREG_VALUE18,
    MISCREG_VALUE19,
    MISCREG_VALUE20,
    MISCREG_VALUE21,
    MISCREG_VALUE22,
    MISCREG_VALUE23,
    MISCREG_VALUE24,
    MISCREG_VALUE25,
    MISCREG_VALUE26,
    MISCREG_VALUE27,
    MISCREG_VALUE28,
    MISCREG_VALUE29,
    MISCREG_VALUE30,
    MISCREG_VALUE31,
    NUM_MISCREGS
};
const int NumMiscRegs = NUM_MISCREGS;

enum CSRIndex {
    CSR_USTATUS = 0x000,
    CSR_UIE = 0x004,
    CSR_UTVEC = 0x005,
    CSR_USCRATCH = 0x040,
    CSR_UEPC = 0x041,
    CSR_UCAUSE = 0x042,
    CSR_UTVAL = 0x043,
    CSR_UIP = 0x044,
    CSR_FFLAGS = 0x001,
    CSR_FRM = 0x002,
    CSR_FCSR = 0x003,
    CSR_CYCLE = 0xC00,
    CSR_TIME = 0xC01,
    CSR_INSTRET = 0xC02,
    CSR_HPMCOUNTER03 = 0xC03,
    CSR_HPMCOUNTER04 = 0xC04,
    CSR_HPMCOUNTER05 = 0xC05,
    CSR_HPMCOUNTER06 = 0xC06,
    CSR_HPMCOUNTER07 = 0xC07,
    CSR_HPMCOUNTER08 = 0xC08,
    CSR_HPMCOUNTER09 = 0xC09,
    CSR_HPMCOUNTER10 = 0xC0A,
    CSR_HPMCOUNTER11 = 0xC0B,
    CSR_HPMCOUNTER12 = 0xC0C,
    CSR_HPMCOUNTER13 = 0xC0D,
    CSR_HPMCOUNTER14 = 0xC0E,
    CSR_HPMCOUNTER15 = 0xC0F,
    CSR_HPMCOUNTER16 = 0xC10,
    CSR_HPMCOUNTER17 = 0xC11,
    CSR_HPMCOUNTER18 = 0xC12,
    CSR_HPMCOUNTER19 = 0xC13,
    CSR_HPMCOUNTER20 = 0xC14,
    CSR_HPMCOUNTER21 = 0xC15,
    CSR_HPMCOUNTER22 = 0xC16,
    CSR_HPMCOUNTER23 = 0xC17,
    CSR_HPMCOUNTER24 = 0xC18,
    CSR_HPMCOUNTER25 = 0xC19,
    CSR_HPMCOUNTER26 = 0xC1A,
    CSR_HPMCOUNTER27 = 0xC1B,
    CSR_HPMCOUNTER28 = 0xC1C,
    CSR_HPMCOUNTER29 = 0xC1D,
    CSR_HPMCOUNTER30 = 0xC1E,
    CSR_HPMCOUNTER31 = 0xC1F,
    // HPMCOUNTERH rv32 only

    CSR_SSTATUS = 0x100,
    CSR_SEDELEG = 0x102,
    CSR_SIDELEG = 0x103,
    CSR_SIE = 0x104,
    CSR_STVEC = 0x105,
    CSR_SSCRATCH = 0x140,
    CSR_SEPC = 0x141,
    CSR_SCAUSE = 0x142,
    CSR_STVAL = 0x143,
    CSR_SIP = 0x144,
    CSR_SATP = 0x180,

    CSR_MVENDORID = 0xF11,
    CSR_MARCHID = 0xF12,
    CSR_MIMPID = 0xF13,
    CSR_MHARTID = 0xF14,
    CSR_MSTATUS = 0x300,
    CSR_MISA = 0x301,
    CSR_MEDELEG = 0x302,
    CSR_MIDELEG = 0x303,
    CSR_MIE = 0x304,
    CSR_MTVEC = 0x305,
    CSR_MSCRATCH = 0x340,
    CSR_MEPC = 0x341,
    CSR_MCAUSE = 0x342,
    CSR_MTVAL = 0x343,
    CSR_MIP = 0x344,
    CSR_PMPCFG0 = 0x3A0,
    // pmpcfg1 rv32 only
    CSR_PMPCFG2 = 0x3A2,
    // pmpcfg3 rv32 only
    CSR_PMPADDR00 = 0x3B0,
    CSR_PMPADDR01 = 0x3B1,
    CSR_PMPADDR02 = 0x3B2,
    CSR_PMPADDR03 = 0x3B3,
    CSR_PMPADDR04 = 0x3B4,
    CSR_PMPADDR05 = 0x3B5,
    CSR_PMPADDR06 = 0x3B6,
    CSR_PMPADDR07 = 0x3B7,
    CSR_PMPADDR08 = 0x3B8,
    CSR_PMPADDR09 = 0x3B9,
    CSR_PMPADDR10 = 0x3BA,
    CSR_PMPADDR11 = 0x3BB,
    CSR_PMPADDR12 = 0x3BC,
    CSR_PMPADDR13 = 0x3BD,
    CSR_PMPADDR14 = 0x3BE,
    CSR_PMPADDR15 = 0x3BF,
    CSR_MCYCLE = 0xB00,
    CSR_MINSTRET = 0xB02,
    CSR_MHPMCOUNTER03 = 0xC03,
    CSR_MHPMCOUNTER04 = 0xC04,
    CSR_MHPMCOUNTER05 = 0xC05,
    CSR_MHPMCOUNTER06 = 0xC06,
    CSR_MHPMCOUNTER07 = 0xC07,
    CSR_MHPMCOUNTER08 = 0xC08,
    CSR_MHPMCOUNTER09 = 0xC09,
    CSR_MHPMCOUNTER10 = 0xC0A,
    CSR_MHPMCOUNTER11 = 0xC0B,
    CSR_MHPMCOUNTER12 = 0xC0C,
    CSR_MHPMCOUNTER13 = 0xC0D,
    CSR_MHPMCOUNTER14 = 0xC0E,
    CSR_MHPMCOUNTER15 = 0xC0F,
    CSR_MHPMCOUNTER16 = 0xC10,
    CSR_MHPMCOUNTER17 = 0xC11,
    CSR_MHPMCOUNTER18 = 0xC12,
    CSR_MHPMCOUNTER19 = 0xC13,
    CSR_MHPMCOUNTER20 = 0xC14,
    CSR_MHPMCOUNTER21 = 0xC15,
    CSR_MHPMCOUNTER22 = 0xC16,
    CSR_MHPMCOUNTER23 = 0xC17,
    CSR_MHPMCOUNTER24 = 0xC18,
    CSR_MHPMCOUNTER25 = 0xC19,
    CSR_MHPMCOUNTER26 = 0xC1A,
    CSR_MHPMCOUNTER27 = 0xC1B,
    CSR_MHPMCOUNTER28 = 0xC1C,
    CSR_MHPMCOUNTER29 = 0xC1D,
    CSR_MHPMCOUNTER30 = 0xC1E,
    CSR_MHPMCOUNTER31 = 0xC1F,
    // MHPMCOUNTERH rv32 only
    CSR_MHPMEVENT03 = 0x323,
    CSR_MHPMEVENT04 = 0x324,
    CSR_MHPMEVENT05 = 0x325,
    CSR_MHPMEVENT06 = 0x326,
    CSR_MHPMEVENT07 = 0x327,
    CSR_MHPMEVENT08 = 0x328,
    CSR_MHPMEVENT09 = 0x329,
    CSR_MHPMEVENT10 = 0x32A,
    CSR_MHPMEVENT11 = 0x32B,
    CSR_MHPMEVENT12 = 0x32C,
    CSR_MHPMEVENT13 = 0x32D,
    CSR_MHPMEVENT14 = 0x32E,
    CSR_MHPMEVENT15 = 0x32F,
    CSR_MHPMEVENT16 = 0x330,
    CSR_MHPMEVENT17 = 0x331,
    CSR_MHPMEVENT18 = 0x332,
    CSR_MHPMEVENT19 = 0x333,
    CSR_MHPMEVENT20 = 0x334,
    CSR_MHPMEVENT21 = 0x335,
    CSR_MHPMEVENT22 = 0x336,
    CSR_MHPMEVENT23 = 0x337,
    CSR_MHPMEVENT24 = 0x338,
    CSR_MHPMEVENT25 = 0x339,
    CSR_MHPMEVENT26 = 0x33A,
    CSR_MHPMEVENT27 = 0x33B,
    CSR_MHPMEVENT28 = 0x33C,
    CSR_MHPMEVENT29 = 0x33D,
    CSR_MHPMEVENT30 = 0x33E,
    CSR_MHPMEVENT31 = 0x33F,

    CSR_TSELECT = 0x7A0,
    CSR_TDATA1 = 0x7A1,
    CSR_TDATA2 = 0x7A2,
    CSR_TDATA3 = 0x7A3,
    CSR_DCSR = 0x7B0,
    CSR_DPC = 0x7B1,
    CSR_DSCRATCH = 0x7B2,

    CSR_STATS_EN  = 0x7C1,
    CSR_STNT_BEGIN_VADDR  = 0x7D0,
    CSR_STNT_END_VADDR  = 0x7D8,
    CSR_STREL_BEGIN_VADDR  = 0x7E0,
    CSR_STREL_END_VADDR  = 0x7E8,
    CSR_BARRIER_INIT  = 0x7F0,
    CSR_BARRIER_WAIT  = 0x7F8,
    CSR_BARRIER_POLL  = 0x790,

    CSR_PROC2MNGR = 0x7C0,
    CSR_MNGR2PROC = 0xFC0,
    CSR_NUMCORES  = 0xFC1,

    CSR_TOGGLE0   = 0x800,
    CSR_TOGGLE1   = 0x801, // Toggle TC3 cache inv/wb
    CSR_TOGGLE2   = 0x802, // Toggle appl runtime
    CSR_TOGGLE3   = 0x803, // Toggle appl enqueue
    CSR_TOGGLE4   = 0x804, // Toggle appl dequeue
    CSR_TOGGLE5   = 0x805, // Toggle sucessful steals
    CSR_TOGGLE6   = 0x806, // Toggle failed steals
    CSR_TOGGLE7   = 0x807,
    CSR_TOGGLE8   = 0x808,
    CSR_TOGGLE9   = 0x809,
    CSR_TOGGLE10  = 0x80A,
    CSR_TOGGLE11  = 0x80B,
    CSR_TOGGLE12  = 0x80C,
    CSR_TOGGLE13  = 0x80D,
    CSR_TOGGLE14  = 0x80E,
    CSR_TOGGLE15  = 0x80F,
    CSR_TOGGLE16  = 0x810,
    CSR_TOGGLE17  = 0x811,
    CSR_TOGGLE18  = 0x812,
    CSR_TOGGLE19  = 0x813,
    CSR_TOGGLE20  = 0x814,
    CSR_TOGGLE21  = 0x815,
    CSR_TOGGLE22  = 0x816,
    CSR_TOGGLE23  = 0x817,
    CSR_TOGGLE24  = 0x818,
    CSR_TOGGLE25  = 0x819,
    CSR_TOGGLE26  = 0x81A,
    CSR_TOGGLE27  = 0x81B,
    CSR_TOGGLE28  = 0x81C,
    CSR_TOGGLE29  = 0x81D,
    CSR_TOGGLE30  = 0x81E,
    CSR_TOGGLE31  = 0x81F,

    CSR_VALUE0   = 0x820,
    CSR_VALUE1   = 0x821,
    CSR_VALUE2   = 0x822,
    CSR_VALUE3   = 0x823,
    CSR_VALUE4   = 0x824,
    CSR_VALUE5   = 0x825,
    CSR_VALUE6   = 0x826,
    CSR_VALUE7   = 0x827,
    CSR_VALUE8   = 0x828,
    CSR_VALUE9   = 0x829,
    CSR_VALUE10  = 0x82A,
    CSR_VALUE11  = 0x82B,
    CSR_VALUE12  = 0x82C,
    CSR_VALUE13  = 0x82D,
    CSR_VALUE14  = 0x82E,
    CSR_VALUE15  = 0x82F, // moyang TC3-TBIW: inv/wb type
    CSR_VALUE16  = 0x830, // moyang TC3-TBIW: inv/wb level
    CSR_VALUE17  = 0x831, // moyang TC3-TBIW: load/store level
    CSR_VALUE18  = 0x832, // moyang TC3-ULI: send ULI req to target
    CSR_VALUE19  = 0x833, // moyang TC3-ULI: send ULI resp back
    CSR_VALUE20  = 0x834, // moyang TC3-ULI: recv ULI resp (ack or nack)
    CSR_VALUE21  = 0x835, // moyang TC3-ULI: recv ULI resp value
    CSR_VALUE22  = 0x836, // moyang TC3-ULI: ULI sender ID
    CSR_VALUE23  = 0x837,
    CSR_VALUE24  = 0x838,
    CSR_VALUE25  = 0x839,
    CSR_VALUE26  = 0x83A,
    CSR_VALUE27  = 0x83B,
    CSR_VALUE28  = 0x83C,
    CSR_VALUE29  = 0x83D,
    CSR_VALUE30  = 0x83E,
    CSR_VALUE31  = 0x83F
};

struct CSRMetadata
{
    const std::string name;
    const int physIndex;
};

const std::map<int, CSRMetadata> CSRData = {
    {CSR_USTATUS, {"ustatus", MISCREG_STATUS}},
    {CSR_UIE, {"uie", MISCREG_IE}},
    {CSR_UTVEC, {"utvec", MISCREG_UTVEC}},
    {CSR_USCRATCH, {"uscratch", MISCREG_USCRATCH}},
    {CSR_UEPC, {"uepc", MISCREG_UEPC}},
    {CSR_UCAUSE, {"ucause", MISCREG_UCAUSE}},
    {CSR_UTVAL, {"utval", MISCREG_UTVAL}},
    {CSR_UIP, {"uip", MISCREG_IP}},
    {CSR_FFLAGS, {"fflags", MISCREG_FFLAGS}},
    {CSR_FRM, {"frm", MISCREG_FRM}},
    {CSR_FCSR, {"fcsr", MISCREG_FFLAGS}}, // Actually FRM << 5 | FFLAGS
    {CSR_CYCLE, {"cycle", MISCREG_CYCLE}},
    {CSR_TIME, {"time", MISCREG_TIME}},
    {CSR_INSTRET, {"instret", MISCREG_INSTRET}},
    {CSR_HPMCOUNTER03, {"hpmcounter03", MISCREG_HPMCOUNTER03}},
    {CSR_HPMCOUNTER04, {"hpmcounter04", MISCREG_HPMCOUNTER04}},
    {CSR_HPMCOUNTER05, {"hpmcounter05", MISCREG_HPMCOUNTER05}},
    {CSR_HPMCOUNTER06, {"hpmcounter06", MISCREG_HPMCOUNTER06}},
    {CSR_HPMCOUNTER07, {"hpmcounter07", MISCREG_HPMCOUNTER07}},
    {CSR_HPMCOUNTER08, {"hpmcounter08", MISCREG_HPMCOUNTER08}},
    {CSR_HPMCOUNTER09, {"hpmcounter09", MISCREG_HPMCOUNTER09}},
    {CSR_HPMCOUNTER10, {"hpmcounter10", MISCREG_HPMCOUNTER10}},
    {CSR_HPMCOUNTER11, {"hpmcounter11", MISCREG_HPMCOUNTER11}},
    {CSR_HPMCOUNTER12, {"hpmcounter12", MISCREG_HPMCOUNTER12}},
    {CSR_HPMCOUNTER13, {"hpmcounter13", MISCREG_HPMCOUNTER13}},
    {CSR_HPMCOUNTER14, {"hpmcounter14", MISCREG_HPMCOUNTER14}},
    {CSR_HPMCOUNTER15, {"hpmcounter15", MISCREG_HPMCOUNTER15}},
    {CSR_HPMCOUNTER16, {"hpmcounter16", MISCREG_HPMCOUNTER16}},
    {CSR_HPMCOUNTER17, {"hpmcounter17", MISCREG_HPMCOUNTER17}},
    {CSR_HPMCOUNTER18, {"hpmcounter18", MISCREG_HPMCOUNTER18}},
    {CSR_HPMCOUNTER19, {"hpmcounter19", MISCREG_HPMCOUNTER19}},
    {CSR_HPMCOUNTER20, {"hpmcounter20", MISCREG_HPMCOUNTER20}},
    {CSR_HPMCOUNTER21, {"hpmcounter21", MISCREG_HPMCOUNTER21}},
    {CSR_HPMCOUNTER22, {"hpmcounter22", MISCREG_HPMCOUNTER22}},
    {CSR_HPMCOUNTER23, {"hpmcounter23", MISCREG_HPMCOUNTER23}},
    {CSR_HPMCOUNTER24, {"hpmcounter24", MISCREG_HPMCOUNTER24}},
    {CSR_HPMCOUNTER25, {"hpmcounter25", MISCREG_HPMCOUNTER25}},
    {CSR_HPMCOUNTER26, {"hpmcounter26", MISCREG_HPMCOUNTER26}},
    {CSR_HPMCOUNTER27, {"hpmcounter27", MISCREG_HPMCOUNTER27}},
    {CSR_HPMCOUNTER28, {"hpmcounter28", MISCREG_HPMCOUNTER28}},
    {CSR_HPMCOUNTER29, {"hpmcounter29", MISCREG_HPMCOUNTER29}},
    {CSR_HPMCOUNTER30, {"hpmcounter30", MISCREG_HPMCOUNTER30}},
    {CSR_HPMCOUNTER31, {"hpmcounter31", MISCREG_HPMCOUNTER31}},

    {CSR_SSTATUS, {"sstatus", MISCREG_STATUS}},
    {CSR_SEDELEG, {"sedeleg", MISCREG_SEDELEG}},
    {CSR_SIDELEG, {"sideleg", MISCREG_SIDELEG}},
    {CSR_SIE, {"sie", MISCREG_IE}},
    {CSR_STVEC, {"stvec", MISCREG_STVEC}},
    {CSR_SSCRATCH, {"sscratch", MISCREG_SSCRATCH}},
    {CSR_SEPC, {"sepc", MISCREG_SEPC}},
    {CSR_SCAUSE, {"scause", MISCREG_SCAUSE}},
    {CSR_STVAL, {"stval", MISCREG_STVAL}},
    {CSR_SIP, {"sip", MISCREG_IP}},
    {CSR_SATP, {"satp", MISCREG_SATP}},

    {CSR_MVENDORID, {"mvendorid", MISCREG_VENDORID}},
    {CSR_MARCHID, {"marchid", MISCREG_ARCHID}},
    {CSR_MIMPID, {"mimpid", MISCREG_IMPID}},
    {CSR_MHARTID, {"mhartid", MISCREG_HARTID}},
    {CSR_MSTATUS, {"mstatus", MISCREG_STATUS}},
    {CSR_MISA, {"misa", MISCREG_ISA}},
    {CSR_MEDELEG, {"medeleg", MISCREG_MEDELEG}},
    {CSR_MIDELEG, {"mideleg", MISCREG_MIDELEG}},
    {CSR_MIE, {"mie", MISCREG_IE}},
    {CSR_MTVEC, {"mtvec", MISCREG_MTVEC}},
    {CSR_MSCRATCH, {"mscratch", MISCREG_MSCRATCH}},
    {CSR_MEPC, {"mepc", MISCREG_MEPC}},
    {CSR_MCAUSE, {"mcause", MISCREG_MCAUSE}},
    {CSR_MTVAL, {"mtval", MISCREG_MTVAL}},
    {CSR_MIP, {"mip", MISCREG_IP}},
    {CSR_PMPCFG0, {"pmpcfg0", MISCREG_PMPCFG0}},
    // pmpcfg1 rv32 only
    {CSR_PMPCFG2, {"pmpcfg2", MISCREG_PMPCFG2}},
    // pmpcfg3 rv32 only
    {CSR_PMPADDR00, {"pmpaddr0", MISCREG_PMPADDR00}},
    {CSR_PMPADDR01, {"pmpaddr1", MISCREG_PMPADDR01}},
    {CSR_PMPADDR02, {"pmpaddr2", MISCREG_PMPADDR02}},
    {CSR_PMPADDR03, {"pmpaddr3", MISCREG_PMPADDR03}},
    {CSR_PMPADDR04, {"pmpaddr4", MISCREG_PMPADDR04}},
    {CSR_PMPADDR05, {"pmpaddr5", MISCREG_PMPADDR05}},
    {CSR_PMPADDR06, {"pmpaddr6", MISCREG_PMPADDR06}},
    {CSR_PMPADDR07, {"pmpaddr7", MISCREG_PMPADDR07}},
    {CSR_PMPADDR08, {"pmpaddr8", MISCREG_PMPADDR08}},
    {CSR_PMPADDR09, {"pmpaddr9", MISCREG_PMPADDR09}},
    {CSR_PMPADDR10, {"pmpaddr10", MISCREG_PMPADDR10}},
    {CSR_PMPADDR11, {"pmpaddr11", MISCREG_PMPADDR11}},
    {CSR_PMPADDR12, {"pmpaddr12", MISCREG_PMPADDR12}},
    {CSR_PMPADDR13, {"pmpaddr13", MISCREG_PMPADDR13}},
    {CSR_PMPADDR14, {"pmpaddr14", MISCREG_PMPADDR14}},
    {CSR_PMPADDR15, {"pmpaddr15", MISCREG_PMPADDR15}},
    {CSR_MCYCLE, {"mcycle", MISCREG_CYCLE}},
    {CSR_MINSTRET, {"minstret", MISCREG_INSTRET}},
    {CSR_MHPMCOUNTER03, {"mhpmcounter03", MISCREG_HPMCOUNTER03}},
    {CSR_MHPMCOUNTER04, {"mhpmcounter04", MISCREG_HPMCOUNTER04}},
    {CSR_MHPMCOUNTER05, {"mhpmcounter05", MISCREG_HPMCOUNTER05}},
    {CSR_MHPMCOUNTER06, {"mhpmcounter06", MISCREG_HPMCOUNTER06}},
    {CSR_MHPMCOUNTER07, {"mhpmcounter07", MISCREG_HPMCOUNTER07}},
    {CSR_MHPMCOUNTER08, {"mhpmcounter08", MISCREG_HPMCOUNTER08}},
    {CSR_MHPMCOUNTER09, {"mhpmcounter09", MISCREG_HPMCOUNTER09}},
    {CSR_MHPMCOUNTER10, {"mhpmcounter10", MISCREG_HPMCOUNTER10}},
    {CSR_MHPMCOUNTER11, {"mhpmcounter11", MISCREG_HPMCOUNTER11}},
    {CSR_MHPMCOUNTER12, {"mhpmcounter12", MISCREG_HPMCOUNTER12}},
    {CSR_MHPMCOUNTER13, {"mhpmcounter13", MISCREG_HPMCOUNTER13}},
    {CSR_MHPMCOUNTER14, {"mhpmcounter14", MISCREG_HPMCOUNTER14}},
    {CSR_MHPMCOUNTER15, {"mhpmcounter15", MISCREG_HPMCOUNTER15}},
    {CSR_MHPMCOUNTER16, {"mhpmcounter16", MISCREG_HPMCOUNTER16}},
    {CSR_MHPMCOUNTER17, {"mhpmcounter17", MISCREG_HPMCOUNTER17}},
    {CSR_MHPMCOUNTER18, {"mhpmcounter18", MISCREG_HPMCOUNTER18}},
    {CSR_MHPMCOUNTER19, {"mhpmcounter19", MISCREG_HPMCOUNTER19}},
    {CSR_MHPMCOUNTER20, {"mhpmcounter20", MISCREG_HPMCOUNTER20}},
    {CSR_MHPMCOUNTER21, {"mhpmcounter21", MISCREG_HPMCOUNTER21}},
    {CSR_MHPMCOUNTER22, {"mhpmcounter22", MISCREG_HPMCOUNTER22}},
    {CSR_MHPMCOUNTER23, {"mhpmcounter23", MISCREG_HPMCOUNTER23}},
    {CSR_MHPMCOUNTER24, {"mhpmcounter24", MISCREG_HPMCOUNTER24}},
    {CSR_MHPMCOUNTER25, {"mhpmcounter25", MISCREG_HPMCOUNTER25}},
    {CSR_MHPMCOUNTER26, {"mhpmcounter26", MISCREG_HPMCOUNTER26}},
    {CSR_MHPMCOUNTER27, {"mhpmcounter27", MISCREG_HPMCOUNTER27}},
    {CSR_MHPMCOUNTER28, {"mhpmcounter28", MISCREG_HPMCOUNTER28}},
    {CSR_MHPMCOUNTER29, {"mhpmcounter29", MISCREG_HPMCOUNTER29}},
    {CSR_MHPMCOUNTER30, {"mhpmcounter30", MISCREG_HPMCOUNTER30}},
    {CSR_MHPMCOUNTER31, {"mhpmcounter31", MISCREG_HPMCOUNTER31}},
    {CSR_MHPMEVENT03, {"mhpmevent03", MISCREG_HPMEVENT03}},
    {CSR_MHPMEVENT04, {"mhpmevent04", MISCREG_HPMEVENT04}},
    {CSR_MHPMEVENT05, {"mhpmevent05", MISCREG_HPMEVENT05}},
    {CSR_MHPMEVENT06, {"mhpmevent06", MISCREG_HPMEVENT06}},
    {CSR_MHPMEVENT07, {"mhpmevent07", MISCREG_HPMEVENT07}},
    {CSR_MHPMEVENT08, {"mhpmevent08", MISCREG_HPMEVENT08}},
    {CSR_MHPMEVENT09, {"mhpmevent09", MISCREG_HPMEVENT09}},
    {CSR_MHPMEVENT10, {"mhpmevent10", MISCREG_HPMEVENT10}},
    {CSR_MHPMEVENT11, {"mhpmevent11", MISCREG_HPMEVENT11}},
    {CSR_MHPMEVENT12, {"mhpmevent12", MISCREG_HPMEVENT12}},
    {CSR_MHPMEVENT13, {"mhpmevent13", MISCREG_HPMEVENT13}},
    {CSR_MHPMEVENT14, {"mhpmevent14", MISCREG_HPMEVENT14}},
    {CSR_MHPMEVENT15, {"mhpmevent15", MISCREG_HPMEVENT15}},
    {CSR_MHPMEVENT16, {"mhpmevent16", MISCREG_HPMEVENT16}},
    {CSR_MHPMEVENT17, {"mhpmevent17", MISCREG_HPMEVENT17}},
    {CSR_MHPMEVENT18, {"mhpmevent18", MISCREG_HPMEVENT18}},
    {CSR_MHPMEVENT19, {"mhpmevent19", MISCREG_HPMEVENT19}},
    {CSR_MHPMEVENT20, {"mhpmevent20", MISCREG_HPMEVENT20}},
    {CSR_MHPMEVENT21, {"mhpmevent21", MISCREG_HPMEVENT21}},
    {CSR_MHPMEVENT22, {"mhpmevent22", MISCREG_HPMEVENT22}},
    {CSR_MHPMEVENT23, {"mhpmevent23", MISCREG_HPMEVENT23}},
    {CSR_MHPMEVENT24, {"mhpmevent24", MISCREG_HPMEVENT24}},
    {CSR_MHPMEVENT25, {"mhpmevent25", MISCREG_HPMEVENT25}},
    {CSR_MHPMEVENT26, {"mhpmevent26", MISCREG_HPMEVENT26}},
    {CSR_MHPMEVENT27, {"mhpmevent27", MISCREG_HPMEVENT27}},
    {CSR_MHPMEVENT28, {"mhpmevent28", MISCREG_HPMEVENT28}},
    {CSR_MHPMEVENT29, {"mhpmevent29", MISCREG_HPMEVENT29}},
    {CSR_MHPMEVENT30, {"mhpmevent30", MISCREG_HPMEVENT30}},
    {CSR_MHPMEVENT31, {"mhpmevent31", MISCREG_HPMEVENT31}},

    {CSR_TSELECT, {"tselect", MISCREG_TSELECT}},
    {CSR_TDATA1, {"tdata1", MISCREG_TDATA1}},
    {CSR_TDATA2, {"tdata2", MISCREG_TDATA2}},
    {CSR_TDATA3, {"tdata3", MISCREG_TDATA3}},
    {CSR_DCSR, {"dcsr", MISCREG_DCSR}},
    {CSR_DPC, {"dpc", MISCREG_DPC}},
    {CSR_DSCRATCH, {"dscratch", MISCREG_DSCRATCH}},

    /*
     * Custom machine-level CSR registers
     */

    {CSR_STATS_EN , { "stats_en"  , MISCREG_STATS_EN   }},
    {CSR_STNT_BEGIN_VADDR , { "stnt_begin_vaddr"  , MISCREG_STNT_BEGIN_VADDR   }},
    {CSR_STNT_END_VADDR , { "stnt_end_vaddr"  , MISCREG_STNT_END_VADDR   }},
    {CSR_STREL_BEGIN_VADDR , { "strel_begin_vaddr"  , MISCREG_STREL_BEGIN_VADDR   }},
    {CSR_STREL_END_VADDR , { "strel_end_vaddr"  , MISCREG_STREL_END_VADDR   }},
    {CSR_BARRIER_INIT , { "barrier_init"  , MISCREG_BARRIER_INIT   }},
    {CSR_BARRIER_WAIT , { "barrier_wait"  , MISCREG_BARRIER_WAIT   }},
    {CSR_BARRIER_POLL , { "barrier_poll"  , MISCREG_BARRIER_POLL   }},
    {CSR_PROC2MNGR, { "proc2mngr" , MISCREG_PROC2MNGR  }},
    {CSR_MNGR2PROC, { "mngr2proc" , MISCREG_MNGR2PROC  }},
    {CSR_NUMCORES , { "numcores"  , MISCREG_NUMCORES   }},

    {CSR_TOGGLE0 , { "toggle_csr_0" , MISCREG_TOGGLE0  }},
    {CSR_TOGGLE1 , { "toggle_csr_1" , MISCREG_TOGGLE1  }},
    {CSR_TOGGLE2 , { "toggle_csr_2" , MISCREG_TOGGLE2  }},
    {CSR_TOGGLE3 , { "toggle_csr_3" , MISCREG_TOGGLE3  }},
    {CSR_TOGGLE4 , { "toggle_csr_4" , MISCREG_TOGGLE4  }},
    {CSR_TOGGLE5 , { "toggle_csr_5" , MISCREG_TOGGLE5  }},
    {CSR_TOGGLE6 , { "toggle_csr_6" , MISCREG_TOGGLE6  }},
    {CSR_TOGGLE7 , { "toggle_csr_7" , MISCREG_TOGGLE7  }},
    {CSR_TOGGLE8 , { "toggle_csr_8" , MISCREG_TOGGLE8  }},
    {CSR_TOGGLE9 , { "toggle_csr_9" , MISCREG_TOGGLE9  }},
    {CSR_TOGGLE10, { "toggle_csr_10", MISCREG_TOGGLE10 }},
    {CSR_TOGGLE11, { "toggle_csr_11", MISCREG_TOGGLE11 }},
    {CSR_TOGGLE12, { "toggle_csr_12", MISCREG_TOGGLE12 }},
    {CSR_TOGGLE13, { "toggle_csr_13", MISCREG_TOGGLE13 }},
    {CSR_TOGGLE14, { "toggle_csr_14", MISCREG_TOGGLE14 }},
    {CSR_TOGGLE15, { "toggle_csr_15", MISCREG_TOGGLE15 }},
    {CSR_TOGGLE16, { "toggle_csr_16", MISCREG_TOGGLE16 }},
    {CSR_TOGGLE17, { "toggle_csr_17", MISCREG_TOGGLE17 }},
    {CSR_TOGGLE18, { "toggle_csr_18", MISCREG_TOGGLE18 }},
    {CSR_TOGGLE19, { "toggle_csr_19", MISCREG_TOGGLE19 }},
    {CSR_TOGGLE20, { "toggle_csr_20", MISCREG_TOGGLE20 }},
    {CSR_TOGGLE21, { "toggle_csr_21", MISCREG_TOGGLE21 }},
    {CSR_TOGGLE22, { "toggle_csr_22", MISCREG_TOGGLE22 }},
    {CSR_TOGGLE23, { "toggle_csr_23", MISCREG_TOGGLE23 }},
    {CSR_TOGGLE24, { "toggle_csr_24", MISCREG_TOGGLE24 }},
    {CSR_TOGGLE25, { "toggle_csr_25", MISCREG_TOGGLE25 }},
    {CSR_TOGGLE26, { "toggle_csr_26", MISCREG_TOGGLE26 }},
    {CSR_TOGGLE27, { "toggle_csr_27", MISCREG_TOGGLE27 }},
    {CSR_TOGGLE28, { "toggle_csr_28", MISCREG_TOGGLE28 }},
    {CSR_TOGGLE29, { "toggle_csr_29", MISCREG_TOGGLE29 }},
    {CSR_TOGGLE30, { "toggle_csr_30", MISCREG_TOGGLE30 }},
    {CSR_TOGGLE31, { "toggle_csr_31", MISCREG_TOGGLE31 }},

    {CSR_VALUE0 , { "value_csr_0" ,  MISCREG_VALUE0 }},
    {CSR_VALUE1 , { "value_csr_1" ,  MISCREG_VALUE1 }},
    {CSR_VALUE2 , { "value_csr_2" ,  MISCREG_VALUE2 }},
    {CSR_VALUE3 , { "value_csr_3" ,  MISCREG_VALUE3 }},
    {CSR_VALUE4 , { "value_csr_4" ,  MISCREG_VALUE4 }},
    {CSR_VALUE5 , { "value_csr_5" ,  MISCREG_VALUE5 }},
    {CSR_VALUE6 , { "value_csr_6" ,  MISCREG_VALUE6 }},
    {CSR_VALUE7 , { "value_csr_7" ,  MISCREG_VALUE7 }},
    {CSR_VALUE8 , { "value_csr_8" ,  MISCREG_VALUE8 }},
    {CSR_VALUE9 , { "value_csr_9" ,  MISCREG_VALUE9 }},
    {CSR_VALUE10, { "value_csr_10",  MISCREG_VALUE10}},
    {CSR_VALUE11, { "value_csr_11",  MISCREG_VALUE11}},
    {CSR_VALUE12, { "value_csr_12",  MISCREG_VALUE12}},
    {CSR_VALUE13, { "value_csr_13",  MISCREG_VALUE13}},
    {CSR_VALUE14, { "value_csr_14",  MISCREG_VALUE14}},
    {CSR_VALUE15, { "value_csr_15",  MISCREG_VALUE15}},
    {CSR_VALUE16, { "value_csr_16",  MISCREG_VALUE16}},
    {CSR_VALUE17, { "value_csr_17",  MISCREG_VALUE17}},
    {CSR_VALUE18, { "value_csr_18",  MISCREG_VALUE18}},
    {CSR_VALUE19, { "value_csr_19",  MISCREG_VALUE19}},
    {CSR_VALUE20, { "value_csr_20",  MISCREG_VALUE20}},
    {CSR_VALUE21, { "value_csr_21",  MISCREG_VALUE21}},
    {CSR_VALUE22, { "value_csr_22",  MISCREG_VALUE22}},
    {CSR_VALUE23, { "value_csr_23",  MISCREG_VALUE23}},
    {CSR_VALUE24, { "value_csr_24",  MISCREG_VALUE24}},
    {CSR_VALUE25, { "value_csr_25",  MISCREG_VALUE25}},
    {CSR_VALUE26, { "value_csr_26",  MISCREG_VALUE26}},
    {CSR_VALUE27, { "value_csr_27",  MISCREG_VALUE27}},
    {CSR_VALUE28, { "value_csr_28",  MISCREG_VALUE28}},
    {CSR_VALUE29, { "value_csr_29",  MISCREG_VALUE29}},
    {CSR_VALUE30, { "value_csr_30",  MISCREG_VALUE30}},
    {CSR_VALUE31, { "value_csr_31",  MISCREG_VALUE31}}
};

/**
 * These fields are specified in the RISC-V Instruction Set Manual, Volume II,
 * v1.10, accessible at www.riscv.org. in Figure 3.7. The main register that
 * uses these fields is the MSTATUS register, which is shadowed by two others
 * accessible at lower privilege levels (SSTATUS and USTATUS) that can't see
 * the fields for higher privileges.
 */
BitUnion64(STATUS)
    Bitfield<63> sd;
    Bitfield<35, 34> sxl;
    Bitfield<33, 32> uxl;
    Bitfield<22> tsr;
    Bitfield<21> tw;
    Bitfield<20> tvm;
    Bitfield<19> mxr;
    Bitfield<18> sum;
    Bitfield<17> mprv;
    Bitfield<16, 15> xs;
    Bitfield<14, 13> fs;
    Bitfield<12, 11> mpp;
    Bitfield<8> spp;
    Bitfield<7> mpie;
    Bitfield<5> spie;
    Bitfield<4> upie;
    Bitfield<3> mie;
    Bitfield<1> sie;
    Bitfield<0> uie;
EndBitUnion(STATUS)

/**
 * These fields are specified in the RISC-V Instruction Set Manual, Volume II,
 * v1.10 in Figures 3.11 and 3.12, accessible at www.riscv.org. Both the MIP
 * and MIE registers have the same fields, so accesses to either should use
 * this bit union.
 */
BitUnion64(INTERRUPT)
    Bitfield<11> mei;
    Bitfield<9> sei;
    Bitfield<8> uei;
    Bitfield<7> mti;
    Bitfield<5> sti;
    Bitfield<4> uti;
    Bitfield<3> msi;
    Bitfield<1> ssi;
    Bitfield<0> usi;
EndBitUnion(INTERRUPT)

const off_t MXL_OFFSET = (sizeof(uint64_t) * 8 - 2);
const off_t SXL_OFFSET = 34;
const off_t UXL_OFFSET = 32;
const off_t FS_OFFSET = 13;
const off_t FRM_OFFSET = 5;

const RegVal ISA_MXL_MASK = 3ULL << MXL_OFFSET;
const RegVal ISA_EXT_MASK = mask(26);
const RegVal MISA_MASK = ISA_MXL_MASK | ISA_EXT_MASK;

const RegVal STATUS_SD_MASK = 1ULL << ((sizeof(uint64_t) * 8) - 1);
const RegVal STATUS_SXL_MASK = 3ULL << SXL_OFFSET;
const RegVal STATUS_UXL_MASK = 3ULL << UXL_OFFSET;
const RegVal STATUS_TSR_MASK = 1ULL << 22;
const RegVal STATUS_TW_MASK = 1ULL << 21;
const RegVal STATUS_TVM_MASK = 1ULL << 20;
const RegVal STATUS_MXR_MASK = 1ULL << 19;
const RegVal STATUS_SUM_MASK = 1ULL << 18;
const RegVal STATUS_MPRV_MASK = 1ULL << 17;
const RegVal STATUS_XS_MASK = 3ULL << 15;
const RegVal STATUS_FS_MASK = 3ULL << FS_OFFSET;
const RegVal STATUS_MPP_MASK = 3ULL << 11;
const RegVal STATUS_SPP_MASK = 1ULL << 8;
const RegVal STATUS_MPIE_MASK = 1ULL << 7;
const RegVal STATUS_SPIE_MASK = 1ULL << 5;
const RegVal STATUS_UPIE_MASK = 1ULL << 4;
const RegVal STATUS_MIE_MASK = 1ULL << 3;
const RegVal STATUS_SIE_MASK = 1ULL << 1;
const RegVal STATUS_UIE_MASK = 1ULL << 0;
const RegVal MSTATUS_MASK = STATUS_SD_MASK | STATUS_SXL_MASK |
                            STATUS_UXL_MASK | STATUS_TSR_MASK |
                            STATUS_TW_MASK | STATUS_TVM_MASK |
                            STATUS_MXR_MASK | STATUS_SUM_MASK |
                            STATUS_MPRV_MASK | STATUS_XS_MASK |
                            STATUS_FS_MASK | STATUS_MPP_MASK |
                            STATUS_SPP_MASK | STATUS_MPIE_MASK |
                            STATUS_SPIE_MASK | STATUS_UPIE_MASK |
                            STATUS_MIE_MASK | STATUS_SIE_MASK |
                            STATUS_UIE_MASK;
const RegVal SSTATUS_MASK = STATUS_SD_MASK | STATUS_UXL_MASK |
                            STATUS_MXR_MASK | STATUS_SUM_MASK |
                            STATUS_XS_MASK | STATUS_FS_MASK |
                            STATUS_SPP_MASK | STATUS_SPIE_MASK |
                            STATUS_UPIE_MASK | STATUS_SIE_MASK |
                            STATUS_UIE_MASK;
const RegVal USTATUS_MASK = STATUS_SD_MASK | STATUS_MXR_MASK |
                            STATUS_SUM_MASK | STATUS_XS_MASK |
                            STATUS_FS_MASK | STATUS_UPIE_MASK |
                            STATUS_UIE_MASK;

const RegVal MEI_MASK = 1ULL << 11;
const RegVal SEI_MASK = 1ULL << 9;
const RegVal UEI_MASK = 1ULL << 8;
const RegVal MTI_MASK = 1ULL << 7;
const RegVal STI_MASK = 1ULL << 5;
const RegVal UTI_MASK = 1ULL << 4;
const RegVal MSI_MASK = 1ULL << 3;
const RegVal SSI_MASK = 1ULL << 1;
const RegVal USI_MASK = 1ULL << 0;
const RegVal MI_MASK = MEI_MASK | SEI_MASK | UEI_MASK |
                       MTI_MASK | STI_MASK | UTI_MASK |
                       MSI_MASK | SSI_MASK | USI_MASK;
const RegVal SI_MASK = SEI_MASK | UEI_MASK |
                       STI_MASK | UTI_MASK |
                       SSI_MASK | USI_MASK;
const RegVal UI_MASK = UEI_MASK | UTI_MASK | USI_MASK;
const RegVal FFLAGS_MASK = (1 << FRM_OFFSET) - 1;
const RegVal FRM_MASK = 0x7;

const std::map<int, RegVal> CSRMasks = {
    {CSR_USTATUS, USTATUS_MASK},
    {CSR_UIE, UI_MASK},
    {CSR_UIP, UI_MASK},
    {CSR_FFLAGS, FFLAGS_MASK},
    {CSR_FRM, FRM_MASK},
    {CSR_FCSR, FFLAGS_MASK | (FRM_MASK << FRM_OFFSET)},
    {CSR_SSTATUS, SSTATUS_MASK},
    {CSR_SIE, SI_MASK},
    {CSR_SIP, SI_MASK},
    {CSR_MSTATUS, MSTATUS_MASK},
    {CSR_MISA, MISA_MASK},
    {CSR_MIE, MI_MASK},
    {CSR_MIP, MI_MASK}
};

}

#endif // __ARCH_RISCV_REGISTERS_HH__