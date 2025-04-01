#!/bin/bash
 
 # SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 # SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 #
 # NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 # property and proprietary rights in and to this material, related
 # documentation and any modifications thereto. Any use, reproduction,
 # disclosure or distribution of this material and related documentation
 # without an express license agreement from NVIDIA CORPORATION or
 # its affiliates is strictly prohibited.


cp -r ./alloy-apps/* ../alloy-apps/
cp -r ./alloy-gem5/* ../alloy-gem5/

cd /artifact_top/alloy-apps/build-applrts_sc3
../configure --host=riscv64-unknown-linux-gnu
make run_allreduce
make run_reduce
make run_scatter
make run_gatther
make run_alltoall
make run_sendrecv
make run_sendrecv_pad
make run_barrier
make PR
make PR_no_local_wt
make SSSP
make SSSP_no_local_wt
make TQH
make PAD
make PAD_RC_CXL
make PAD_RC_UPI
make PAD_no_local_wt
make HSTI
make TRNS

MAX_CORES=$(nproc)
cd /artifact_top/alloy-gem5/

# set RC sequencer
cp src/mem/ruby/system/DOSequencerRC.py src/mem/ruby/system/DOSequencer.py
cp src/mem/ruby/system/DOSequencerRC.hh src/mem/ruby/system/DOSequencer.hh
cp src/mem/ruby/system/DOSequencerRC.cc src/mem/ruby/system/DOSequencer.cc

# set RC WT
cp src/mem/protocol/WT_MESI-L1cache_RC.sm src/mem/protocol/WT_MESI-L1cache.sm

# 8B PUT_NT
mkdir -p build/RISCV_DO_MESI
mkdir -p build/RISCV_WT_MESI
mkdir -p build/RISCV_MESI
mkdir -p build/RISCV_MP
cp src/mem/ruby/network/Network_8B.cc src/mem/ruby/network/Network.cc
scons build/RISCV_MP/gem5.opt --default=RISCV PROTOCOL=MP -j$MAX_CORES
scons build/RISCV_DO_MESI/gem5.opt --default=RISCV PROTOCOL=DO_MESI -j$MAX_CORES
scons build/RISCV_WT_MESI/gem5.opt --default=RISCV PROTOCOL=WT_MESI -j$MAX_CORES
scons build/RISCV_MESI/gem5.opt --default=RISCV PROTOCOL=MESI -j$MAX_CORES

# 64B PUT_NT
mkdir -p build/RISCV_DO_MESI_64B
mkdir -p build/RISCV_WT_MESI_64B
mkdir -p build/RISCV_MP_64B
cp src/mem/ruby/network/Network_64B.cc src/mem/ruby/network/Network.cc
scons build/RISCV_MP_64B/gem5.opt --default=RISCV PROTOCOL=MP -j$MAX_CORES
scons build/RISCV_DO_MESI_64B/gem5.opt --default=RISCV PROTOCOL=DO_MESI -j$MAX_CORES
scons build/RISCV_WT_MESI_64B/gem5.opt --default=RISCV PROTOCOL=WT_MESI -j$MAX_CORES

# 256B PUT_NT
mkdir -p build/RISCV_DO_MESI_256B
mkdir -p build/RISCV_WT_MESI_256B
mkdir -p build/RISCV_MP_256B
cp src/mem/ruby/network/Network_256B.cc src/mem/ruby/network/Network.cc
scons build/RISCV_MP_256B/gem5.opt --default=RISCV PROTOCOL=MP -j$MAX_CORES
scons build/RISCV_DO_MESI_256B/gem5.opt --default=RISCV PROTOCOL=DO_MESI -j$MAX_CORES
scons build/RISCV_WT_MESI_256B/gem5.opt --default=RISCV PROTOCOL=WT_MESI -j$MAX_CORES

# 1KB PUT_NT
mkdir -p build/RISCV_DO_MESI_1KB
mkdir -p build/RISCV_WT_MESI_1KB
mkdir -p build/RISCV_MP_1KB
cp src/mem/ruby/network/Network_1KB.cc src/mem/ruby/network/Network.cc
scons build/RISCV_MP_1KB/gem5.opt --default=RISCV PROTOCOL=MP -j$MAX_CORES
scons build/RISCV_DO_MESI_1KB/gem5.opt --default=RISCV PROTOCOL=DO_MESI -j$MAX_CORES
scons build/RISCV_WT_MESI_1KB/gem5.opt --default=RISCV PROTOCOL=WT_MESI -j$MAX_CORES

# 4KB PUT_NT
mkdir -p build/RISCV_DO_MESI_4KB
mkdir -p build/RISCV_WT_MESI_4KB
mkdir -p build/RISCV_MP_4KB
cp src/mem/ruby/network/Network_4KB.cc src/mem/ruby/network/Network.cc
scons build/RISCV_MP_4KB/gem5.opt --default=RISCV PROTOCOL=MP -j$MAX_CORES
scons build/RISCV_DO_MESI_4KB/gem5.opt --default=RISCV PROTOCOL=DO_MESI -j$MAX_CORES
scons build/RISCV_WT_MESI_4KB/gem5.opt --default=RISCV PROTOCOL=WT_MESI -j$MAX_CORES


# set TSO sequencer
cp src/mem/ruby/system/DOSequencerTSO.py src/mem/ruby/system/DOSequencer.py
cp src/mem/ruby/system/DOSequencerTSO.hh src/mem/ruby/system/DOSequencer.hh
cp src/mem/ruby/system/DOSequencerTSO.cc src/mem/ruby/system/DOSequencer.cc

# set TSO WT
cp src/mem/protocol/WT_MESI-L1cache_TSO.sm src/mem/protocol/WT_MESI-L1cache.sm

# 8B PUT_REL
mkdir -p build/RISCV_DO_MESI_TSO
mkdir -p build/RISCV_WT_MESI_TSO
mkdir -p build/RISCV_MESI_TSO
mkdir -p build/RISCV_MP_TSO
cp src/mem/ruby/network/Network_8B_TSO.cc src/mem/ruby/network/Network.cc
scons build/RISCV_MP_TSO/gem5.opt --default=RISCV PROTOCOL=MP -j$MAX_CORES
scons build/RISCV_DO_MESI_TSO/gem5.opt --default=RISCV PROTOCOL=DO_MESI -j$MAX_CORES
scons build/RISCV_WT_MESI_TSO/gem5.opt --default=RISCV PROTOCOL=WT_MESI -j$MAX_CORES

cp src/mem/ruby/system/DOSequencerTSOWB.cc src/mem/ruby/system/DOSequencer.cc
scons build/RISCV_MESI_TSO/gem5.opt --default=RISCV PROTOCOL=MESI -j$MAX_CORES

cp src/mem/ruby/system/DOSequencerTSO.cc src/mem/ruby/system/DOSequencer.cc

# 64B PUT_REL
mkdir -p build/RISCV_DO_MESI_64B_TSO
mkdir -p build/RISCV_WT_MESI_64B_TSO
mkdir -p build/RISCV_MP_64B_TSO
cp src/mem/ruby/network/Network_64B_TSO.cc src/mem/ruby/network/Network.cc
scons build/RISCV_MP_64B_TSO/gem5.opt --default=RISCV PROTOCOL=MP -j$MAX_CORES
scons build/RISCV_DO_MESI_64B_TSO/gem5.opt --default=RISCV PROTOCOL=DO_MESI -j$MAX_CORES
scons build/RISCV_WT_MESI_64B_TSO/gem5.opt --default=RISCV PROTOCOL=WT_MESI -j$MAX_CORES