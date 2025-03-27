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

exp_id=500
latencies=("25" "50" "75" "100" "124" "150" "175" "200") # note that the data point is supposed to be 124 but just make it 125 to make one weird case runnable
syss=("DO_MESI" "WT_MESI" "MP")

sync_grans=("16" "128" "1024" "8192" "65536") # in floats
for sync_gran in "${sync_grans[@]}"; do
    for sys in "${syss[@]}"; do
        for latency in "${latencies[@]}"; do
            nohup /artifact_top/alloy-gem5/build/RISCV_${sys}_64B/gem5.opt --stats-file=stats${exp_id}.txt --debug-flags=DOMEM,DOACC --listener-mode=off /artifact_top/alloy-gem5/configs/brg/sc3.py --cpu-type TimingSimpleCPU --link-latency 20 --CG_link_latency ${latency} --garnet-deadlock-threshold 100000 --l2_size 16384kB --l2_assoc 8 --num-dirs 8 --brg-fast-forward --num-cpus 64 --network garnet2.0 --buffer-size 0 --mem-size 4GB --ruby --l1d_size 64kB --mem-type SimpleMemory --num-l1-cache-ports 8 --num-l2caches 8 --ports 16 --l1i_size 64kB --topology TwoMeshXY -c /artifact_top/alloy-apps/build-applrts_sc3/run_sendrecv_pad -o "-w 0 -r 1 -n 2 -c 8 -f ${sync_gran} -s 8 -p 21" --vcs-per-vnet 64 --buffers-per-data-vc 65536 --buffers-per-ctrl-vc 1024 > logs/log${exp_id}.txt &
            exp_id=$((exp_id + 1))
        done
    done
done

sync_grans=("16" "1024")
fanouts=("2" "4" "8")
for sync_gran in "${sync_grans[@]}"; do # additionally test for small sync gran for cut-off point
    for fanout in "${fanouts[@]}"; do
        for sys in "${syss[@]}"; do
            for latency in "${latencies[@]}"; do
                nohup /artifact_top/alloy-gem5/build/RISCV_${sys}_64B/gem5.opt --stats-file=stats${exp_id}.txt --debug-flags=DOMEM,DOACC --listener-mode=off /artifact_top/alloy-gem5/configs/brg/sc3.py --cpu-type TimingSimpleCPU --link-latency 20 --CG_link_latency ${latency} --garnet-deadlock-threshold 100000 --l2_size 16384kB --l2_assoc 8 --num-dirs 8 --brg-fast-forward --num-cpus 64 --network garnet2.0 --buffer-size 0 --mem-size 4GB --ruby --l1d_size 64kB --mem-type SimpleMemory --num-l1-cache-ports 8 --num-l2caches 8 --ports 16 --l1i_size 64kB --topology TwoMeshXY -c /artifact_top/alloy-apps/build-applrts_sc3/run_sendrecv_pad -o "-w 0 -r 1 -n ${fanout} -c 8 -f ${sync_gran} -s 8 -p 21" --vcs-per-vnet 64 --buffers-per-data-vc 65536 --buffers-per-ctrl-vc 1024 > logs/log${exp_id}.txt &
                exp_id=$((exp_id + 1))
            done
        done
    done
done

store_grans=("" "_64B" "_256B" "_1KB" "_4KB")
strides=("1" "8" "32" "128" "512")
for ((i=0; i<${#store_grans[@]}; i++)); do
    store_gran=${store_grans[i]}
    stride=${strides[i]}
    for sys in "${syss[@]}"; do
        for latency in "${latencies[@]}"; do
            nohup /artifact_top/alloy-gem5/build/RISCV_${sys}${store_gran}/gem5.opt --stats-file=stats${exp_id}.txt --debug-flags=DOMEM,DOACC --listener-mode=off /artifact_top/alloy-gem5/configs/brg/sc3.py --cpu-type TimingSimpleCPU --link-latency 20 --CG_link_latency ${latency} --garnet-deadlock-threshold 100000 --l2_size 16384kB --l2_assoc 8 --num-dirs 8 --brg-fast-forward --num-cpus 64 --network garnet2.0 --buffer-size 0 --mem-size 4GB --ruby --l1d_size 64kB --mem-type SimpleMemory --num-l1-cache-ports 8 --num-l2caches 8 --ports 16 --l1i_size 64kB --topology TwoMeshXY -c /artifact_top/alloy-apps/build-applrts_sc3/run_sendrecv_pad -o "-w 0 -r 1 -n 2 -c 8 -f 1024 -s ${stride} -p 21" --vcs-per-vnet 64 --buffers-per-data-vc 65536 --buffers-per-ctrl-vc 1024 > logs/log${exp_id}.txt &
            exp_id=$((exp_id + 1))
        done
    done
done
wait