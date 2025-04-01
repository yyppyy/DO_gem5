# SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION &
# AFFILIATES. All rights reserved.
# SPDX-License-Identifier: LicenseRef-NvidiaProprietary
#
# NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
# property and proprietary rights in and to this material, related
# documentation and any modifications thereto. Any use, reproduction,
# disclosure or distribution of this material and related documentation
# without an express license agreement from NVIDIA CORPORATION or
# its affiliates is strictly prohibited.

import argparse
import re
import math
import os

def copy_results(docker_hash):
    os.system(f'sudo docker cp {docker_hash}:/artifact_top/DO_gem5/logs/. /home/yanpeng/NVIDIA/DO_gem5/results/logs/')
    os.system(f'sudo docker cp {docker_hash}:/artifact_top/DO_gem5/m5out/. /home/yanpeng/NVIDIA/DO_gem5/results/m5out/')
    
def find_flits_count(statsfile, flit_size):
    general_traffic = 0
    target_traffic = 0
    with open(statsfile, 'r') as stats:
        for line in stats:
            if "system.ruby.network.CG_link_flits_count" in line:
                parts = line.split()
                if len(parts) == 2 and parts[1].isdigit():
                    X = int(parts[1])
                    general_traffic = X * flit_size
                continue
            if "system.ruby.network.CG_link_flits_bytes" in line:
                parts = line.split()
                if len(parts) == 2 and parts[1].isdigit():
                    X = int(parts[1])
                    target_traffic = X * flit_size
                continue
    return general_traffic, target_traffic

def process_logfile_comp_time_end_to_end(logfile):
    begin_tick = 0
    end_tick = 0
    
    with open(logfile, 'r') as log:
        for line in log:
            match = re.search(r'@ tick (\d+) ', line)
            if match:
                if begin_tick == 0:
                    begin_tick = int(match.group(1))
                else:
                    end_tick = int(match.group(1))
                    break

    return (end_tick - begin_tick) / 1e6

def process_logfile_comp_time_strel_ldacq(logfile, r):
    st_rels = []  # Stores a list of st-rel lines for each N
    last_ld_acq = -1  # Stores the last ld-acq line for each N

    with open(logfile, 'r') as log:
        for line in log:
            if "st-rel" in line:
                st_rels.append(int(line.split()[0][:-1]))

            elif "ld-acq" in line:
                last_ld_acq = int(line.split()[0][:-1])

    return (last_ld_acq - st_rels[len(st_rels) // r * (r - 1)]) / 1e6

def process_logfile_comp_time_stnt_ldnt(logfile, r):
    st_rels = []  # Stores a list of st-rel lines for each N
    last_ld_acq = -1  # Stores the last ld-acq line for each N

    with open(logfile, 'r') as log:
        for line in log:
            if "st-nt" in line:
                st_rels.append(int(line.split()[0][:-1]))

            elif "ld-nt" in line:
                last_ld_acq = int(line.split()[0][:-1])

    return (last_ld_acq - st_rels[len(st_rels) // r * (r - 1)]) / 1e6

def process_logfile_comp_time_lat_cutoff(logfile, latency):
    st_nts = []
    ld_nts = []
    ld_acq = -1
    st_rel_comms = []
    try:
        with open(logfile, 'r') as log:
            for line in log:
                if "st-nt" in line:
                    st_nts.append(int(line.split()[0][:-1]))
                elif "ld-nt" in line:
                    ld_nts.append(int(line.split()[0][:-1]))
                elif "ld-acq" in line:
                    ld_acq = int(line.split()[0][:-1])
                elif "STREL committed" in line:
                    st_rel_comms.append(int(line.split()[0][:-1]))
        # st_time = st_nts[-1] - st_nts[0] + int(latency)
        expected_st_rel_comms = 21
        avg_interval = sum([(st_rel_comms[i+1] - st_rel_comms[i]) for i in range(len(st_rel_comms) - 1)]) / len(st_rel_comms)
        st_rel_comm = st_rel_comms[-1] + avg_interval * (expected_st_rel_comms - len(st_rel_comms))
        st_time = st_rel_comm - st_nts[0]
        # ld_time = ld_nts[-1] - ld_nts[0]
        ld_time = ld_nts[-1] - ld_acq + 1400000 # other run overheads aligned to log649.txt
        return st_time + ld_time
        # return st_time
    except Exception as e:
        return 0
    

def process_logfile_comp_time_ldacq_ldnt(logfile, r):
    st_rels = []  # Stores a list of st-rel lines for each N
    last_ld_acq = -1  # Stores the last ld-acq line for each N

    with open(logfile, 'r') as log:
        for line in log:
            if "ld-acq" in line:
                st_rels.append(int(line.split()[0][:-1]))

            elif "ld-nt" in line:
                last_ld_acq = int(line.split()[0][:-1])

    return (last_ld_acq - st_rels[len(st_rels) // r * (r - 1)]) / 1e6

def process_logfile_comp_time_ldnt_ldacq(logfile, r):
    st_rels = []  # Stores a list of st-rel lines for each N
    last_ld_acq = -1  # Stores the last ld-acq line for each N

    with open(logfile, 'r') as log:
        for line in log:
            if "ld-nt" in line:
                st_rels.append(int(line.split()[0][:-1]))

            elif "ld-acq" in line:
                last_ld_acq = int(line.split()[0][:-1])

    return (last_ld_acq - st_rels[len(st_rels) // r * (r - 1)]) / 1e6

def process_logfile_comp_time_ldnt_strel(logfile, r):
    st_rels = []  # Stores a list of st-rel lines for each N
    last_ld_acq = -1  # Stores the last ld-acq line for each N

    with open(logfile, 'r') as log:
        for line in log:
            if "ld-nt" in line:
                st_rels.append(int(line.split()[0][:-1]))

            elif "st-rel" in line:
                last_ld_acq = int(line.split()[0][:-1])

    return (last_ld_acq - st_rels[len(st_rels) // r * (r - 1)]) / 1e6

def process_logfile_comp_time_ldacq_strel(logfile, r):
    st_rels = []  # Stores a list of st-rel lines for each N
    last_ld_acq = -1  # Stores the last ld-acq line for each N

    with open(logfile, 'r') as log:
        for line in log:
            if "ld-acq" in line:
                st_rels.append(int(line.split()[0][:-1]))

            elif "st-rel" in line:
                last_ld_acq = int(line.split()[0][:-1])

    return (last_ld_acq - st_rels[len(st_rels) // r * (r - 1)]) / 1e6

def process_logfile_comp_time_strel_ldacq_per_thread(logfile, r):
    st_rel_map = {}  # Stores a list of st-rel lines for each N
    last_ld_acq_map = {}  # Stores the last ld-acq line for each N

    with open(logfile, 'r') as log:
        for line in log:
            if "system.ruby.l1_cntrl" in line:
                parts = line.split("system.ruby.l1_cntrl")
                if len(parts) > 1:
                    suffix = parts[1].split(".sequencer")[0]
                    if suffix.isdigit():
                        N = int(suffix)

                        if "st-rel" in line:
                            timestamp = int(line.split()[0][:-1])
                            if N not in st_rel_map:
                                st_rel_map[N] = []
                            st_rel_map[N].append(timestamp)

                        elif "ld-acq" in line:
                            timestamp = int(line.split()[0][:-1])
                            last_ld_acq_map[N] = timestamp

    ld_st_diffs = []
    for N in st_rel_map:
        if N in last_ld_acq_map and len(st_rel_map[N]) > 1:
            if N == 0:
                continue
            L1_t = last_ld_acq_map[N]
            middle_st_rel_index = len(st_rel_map[N]) // r * (r - 1)
            L2_t = st_rel_map[N][middle_st_rel_index]
            ld_st_diffs.append(L1_t - L2_t)

    avg_ld_st_diff = None
    if ld_st_diffs:
        avg_ld_st_diff = sum(ld_st_diffs) / len(ld_st_diffs) / 1000000.0

    return avg_ld_st_diff

def calculate_storage(logfile, stCnt_bw, epoch_bw, n_procs, n_dirs):
    max_pendingEpochs_entries = 0
    max_stCnts_entries = 0
    max_notiCnts_entries = 0
    max_recycled_st_rel = 0
    
    with open(logfile, 'r') as log:
        for line in log:
            match = re.search(r'\b\w+\s+(\d+)\b', line)
            if 'unCommittedEpochs' in line:
                size = int(match.group(1))
                if size > max_pendingEpochs_entries:
                    max_pendingEpochs_entries = size
            elif 'notiCnts' in line:
                size = int(match.group(1))
                if size > max_notiCnts_entries:
                    max_notiCnts_entries = size
            elif 'stCnts' in line:
                size = int(match.group(1))
                if size > max_stCnts_entries:
                    max_stCnts_entries = size
            elif 'recycledStRel' in line:
                size = int(match.group(1))
                if size > max_recycled_st_rel:
                    max_recycled_st_rel = size
    
    dir_bw = (n_dirs - 1).bit_length()
    proc_bw = (n_procs - 1).bit_length()
    st_rel_pkt_bw = 256
    
    proc_epoch_bits = epoch_bw
    proc_stCnt_bits = stCnt_bw * n_dirs
    proc_pendingEpochs_bits = max_pendingEpochs_entries * (dir_bw + epoch_bw)
    
    dir_stCnt_bits = n_procs * max_stCnts_entries * (proc_bw + stCnt_bw)
    dir_notiCnt_bits = n_procs * max_notiCnts_entries * (proc_bw + dir_bw)
    dir_maxCommittedEpochs_bits = n_procs * epoch_bw
    
    dir_network_buffer_bits = max_recycled_st_rel * st_rel_pkt_bw
        
    return math.ceil(proc_epoch_bits / 8),\
        math.ceil(proc_stCnt_bits / 8),\
        math.ceil(proc_pendingEpochs_bits / 8),\
        math.ceil(dir_stCnt_bits / 8),\
        math.ceil(dir_notiCnt_bits / 8),\
        math.ceil(dir_maxCommittedEpochs_bits / 8),\
        math.ceil(dir_network_buffer_bits / 8)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Process log and stats files")
    parser.add_argument('--bench', required=True, help="Path to the log file")
    parser.add_argument('--logfile', required=True, help="Path to the log file")
    parser.add_argument('--statsfile', required=True, help="Path to the stats file")
    parser.add_argument('--flit_size', type=int, default=16, help="Flit size (default is 16)")
    parser.add_argument('--rounds', type=int)
    parser.add_argument('--perf_traffic', action='store_true')
    parser.add_argument('--storage', action='store_true')
    parser.add_argument('--n_PUs', type=int, default=8)
    
    args = parser.parse_args()

    # Process the logfile
    if args.perf_traffic:
        avg_ld_st_diff = 0
        if args.bench in ("allreduce", "alltoall", ):
            avg_ld_st_diff = process_logfile_comp_time_strel_ldacq_per_thread(args.logfile, args.rounds)
        elif args.bench in ("scatter", "gather", "reduce", "sssp", "barrier", ):
            avg_ld_st_diff = process_logfile_comp_time_strel_ldacq(args.logfile, args.rounds)
        elif args.bench in ("pagerank", "pad", ):
            avg_ld_st_diff = process_logfile_comp_time_ldnt_ldacq(args.logfile, args.rounds)
        elif args.bench in ("tqh", ):
            avg_ld_st_diff = process_logfile_comp_time_ldacq_ldnt(args.logfile, args.rounds)
        elif args.bench in ("sendrecv", ):
            avg_ld_st_diff = process_logfile_comp_time_stnt_ldnt(args.logfile, args.rounds)
        else:
            assert(0)

        if avg_ld_st_diff is not None:
            print(f"Average ld-st difference: {avg_ld_st_diff:.6f} us")
        else:
            print("Could not find required ld-acq or st-rel lines in the log file.")

        general_traffic, target_traffic = find_flits_count(args.statsfile, args.flit_size)
        
        if general_traffic is not None:
            print(f"inter-PU Traffic: general: {general_traffic} B, target: {target_traffic} B")
        else:
            print("Line with 'system.ruby.network.CG_link_flits_count' not found.")
    
    if args.storage:
        stCnt_bw = 32
        epoch_bw = 8
        n_procs = args.n_PUs * 8
        n_dirs = args.n_PUs
        proc_epoch, proc_stCnt, proc_pendingEpochs, dir_stCnt, dir_notiCnt, dir_maxCommittedEpochs, dir_network_buffer = calculate_storage(args.logfile, stCnt_bw, epoch_bw, n_procs, n_dirs)
        print(f"Storage overheads(B) proc_epoch {proc_epoch}, proc_stCnt {proc_stCnt}, proc_pendingEpochs {proc_pendingEpochs}, dir_stCnt {dir_stCnt}, dir_notiCnt {dir_notiCnt}, dir_maxCommittedEpochs {dir_maxCommittedEpochs}, dir_network_buffer {dir_network_buffer}")