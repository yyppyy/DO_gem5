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

import post_process as pp
import itertools
import pandas as pd

def dummy(x, y):
    return 0

def get_next_file_id(old_file_id):
    if old_file_id == 1129: # end TQH
        return 1215 # HSTI
    elif old_file_id == 1218: # end HSTI
        return 1231 # TRNS
    elif old_file_id == 1234: # end TRNS
        return 1130 # TQH-next
    elif old_file_id == 1182: # end TQH
        return 1219 # HSTI
    elif old_file_id == 1222: # end HSTI
        return 1235 # TRNS
    elif old_file_id == 1238: # end TRNS
        return 1183 # TQH-next
    return old_file_id + 1

if __name__ == "__main__":
    
    syss = ['DO', 'SO', 'WB', 'MP']
    n_nodes = '8'
    links = ['CXL', 'UPI']
    
    runs = ['SSSP', 'SSSP_warmup',
            'PR', 'PR_warmup',
            'PAD', 'TQH', 'HSTI', 'TRNS',
            'alltoall_10KB', 'reduce_256B',
            'reduce_14KB', 'allreduce_8B',
            'bcast_8B', 'sendrecv_8B',
            'sendrecv_512B', 'barrier']
    
    run_to_profile_func = {
        'SSSP': pp.process_logfile_comp_time_strel_ldacq,
        'SSSP_warmup': dummy,
        'PR': pp.process_logfile_comp_time_ldnt_ldacq,
        'PR_warmup': dummy,
        'TRNS': pp.process_logfile_comp_time_ldacq_strel,
        'PAD': pp.process_logfile_comp_time_ldnt_ldacq,
        'TQH': pp.process_logfile_comp_time_ldacq_ldnt,
        'HSTI': pp.process_logfile_comp_time_ldnt_strel,
        'alltoall_10KB': pp.process_logfile_comp_time_strel_ldacq_per_thread,
        'reduce_256B': pp.process_logfile_comp_time_strel_ldacq,
        'reduce_14KB': pp.process_logfile_comp_time_strel_ldacq,
        'allreduce_8B': pp.process_logfile_comp_time_strel_ldacq_per_thread,
        'bcast_8B': pp.process_logfile_comp_time_strel_ldacq,
        'sendrecv_8B': pp.process_logfile_comp_time_stnt_ldnt,
        'sendrecv_512B': pp.process_logfile_comp_time_stnt_ldnt,
        'barrier': pp.process_logfile_comp_time_strel_ldacq,
    }

    run_to_rounds = {
        'SSSP': 2,
        'SSSP_warmup': 2,
        'PR': 1,
        'PR_warmup': 1,
        'PAD': 1,
        'TQH': 2,
        'HSTI': 1,
        'TRNS': 1,
        'alltoall_10KB': 1,
        'reduce_256B': 1,
        'reduce_14KB': 1,
        'allreduce_8B': 2,
        'bcast_8B': 2,
        'sendrecv_8B': 2,
        'sendrecv_512B': 2,
        'barrier': 2,
    }
        
    file_id = 1109
    data = {}
    for link in links:
        for run in runs:
            for sys in (syss if run != 'PR_warmup' else ['DO']):
                exec_time = run_to_profile_func[run](f'../results/logs/log{file_id}.txt',
                        run_to_rounds[run])
                all_traffic, wt_traffic = pp.find_flits_count(f'../results/m5out/stats{file_id}.txt', 16)
                data[f'{run} {sys} {link}'] = {}
                d = data[f'{run} {sys} {link}']
                d['exec_time'] = exec_time
                d['all_traffic'] = all_traffic
                d['wt_traffic'] = wt_traffic
                file_id = get_next_file_id(file_id)
    
    apps = ['PR', 'SSSP', 'PAD', 'TQH', 'HSTI', 'TRNS',
        'MOCFE', 'CMC-2D', 'BigFFT', 'CR']
    
    measures = ['performance', 'traffic']
    MPI_comp_time = {}
    rows = []
    
    for link in links:
        for app in apps:
            row = [f'{app} {link}',]
            for sys in syss:
                if app in ('PR', 'SSSP', 'PAD', 'TQH', 'HSTI', 'TRNS'):
                    d = data[f'{app} {sys} {link}']
                    run_exec_time = d['exec_time']
                    run_all_traffic = d['all_traffic']
                    run_wt_traffic = d['wt_traffic']
                    if app == 'PR':
                        exec_time = run_exec_time
                        traffic = run_all_traffic - data[f'PR_warmup DO {link}']['all_traffic']
                        if sys == 'MP':
                            traffic *= 1/2
                    elif app == 'SSSP':
                        exec_time = run_exec_time
                        traffic = (run_all_traffic - data[f'SSSP_warmup {sys} {link}']['all_traffic']) if sys != 'SO' else (data[f'SSSP MP {link}']['all_traffic'] - data[f'SSSP_warmup MP {link}']['all_traffic'] + 16 * 18681) # hardcoded flit_size * # st-nt
                        if sys == 'MP':
                            traffic *= 1/2
                    # traffic profiling for PAD and TQH seems to be inaccurate
                    # as realized when writing the script. Fix later
                    # they use total traffic rather than target inter-PU traffic
                    # which could include some initialization time traffic
                    elif app == 'PAD':
                        exec_time = run_exec_time
                        traffic = run_all_traffic if sys != 'SO' else (data[f'PAD MP {link}']['all_traffic'] + 16 * 8071) # hardcoded flit_size * # st-nt
                        if sys == 'MP':
                            traffic *= 6/7
                    elif app == 'TQH':
                        exec_time = run_exec_time
                        traffic = run_all_traffic if sys != 'SO' else (data[f'TQH MP {link}']['all_traffic'] + 16 * 336) # hardcoded flit_size * # st-nt
                        if sys == 'MP':
                            traffic *= 6/7
                    elif app == 'HSTI':
                        exec_time = run_exec_time
                        traffic = run_all_traffic if sys != 'SO' else (data[f'HSTI MP {link}']['all_traffic'])
                        if sys == 'MP':
                            traffic *= 6/7
                    elif app == 'TRNS':
                        exec_time = run_exec_time
                        traffic = run_all_traffic if sys != 'SO' else (data[f'TRNS MP {link}']['all_traffic'])
                        if sys == 'MP':
                            traffic *= 6/7
                else:
                    if app == 'CMC-2D':
                        comm_fact = 0.76
                        comm_time = 0.247 * data[f'reduce_14KB {sys} {link}']['exec_time']\
                            + 0.753 * data[f'barrier {sys} {link}']['exec_time']
                        if sys != 'WB':
                            traffic = 1 * data[f'reduce_14KB {sys} {link}']['wt_traffic'] / 2
                            if sys == 'MP':
                                traffic *= 6/7
                        else:
                            traffic = 1 * (data[f'reduce_14KB WB {link}']['all_traffic'] - data[f'reduce_14KB DO {link}']['all_traffic'] + data[f'reduce_14KB DO {link}']['wt_traffic']) / 2
                        if sys == 'DO':
                            MPI_comp_time[f'CMC-2D {link}'] = comm_time * (1 - comm_fact) / comm_fact
                        exec_time = comm_time + MPI_comp_time[f'CMC-2D {link}']
                    elif app == 'MOCFE':
                        comm_fact = 0.74
                        comm_time = 0.202 * data[f'reduce_256B {sys} {link}']['exec_time']\
                            + 0.312 * data[f'allreduce_8B {sys} {link}']['exec_time']\
                            + 0.351 * data[f'bcast_8B {sys} {link}']['exec_time']\
                            + 0.135 * data[f'sendrecv_8B {sys} {link}']['exec_time']
                        if sys != 'WB':
                            traffic = 0.516 * data[f'reduce_256B {sys} {link}']['wt_traffic'] / 2 * (6/7 if sys == 'MP' else 1)\
                                + 0.455 * data[f'allreduce_8B {sys} {link}']['wt_traffic'] / 2 * (1/2 if sys == 'MP' else 1)\
                                + 0.029 * data[f'sendrecv_8B {sys} {link}']['wt_traffic'] / 2 * (1/2 if sys == 'MP' else 1)
                        else:
                            traffic = 0.516 * (data[f'reduce_256B WB {link}']['all_traffic'] - data[f'reduce_256B DO {link}']['all_traffic'] + data[f'reduce_256B DO {link}']['wt_traffic']) / 2\
                                + 0.455 * (data[f'allreduce_8B WB {link}']['all_traffic'] - data[f'allreduce_8B DO {link}']['all_traffic'] + data[f'allreduce_8B DO {link}']['wt_traffic']) / 2\
                                + 0.029 * (data[f'sendrecv_8B WB {link}']['all_traffic'] - data[f'sendrecv_8B DO {link}']['all_traffic'] + data[f'sendrecv_8B DO {link}']['wt_traffic']) / 2
                        if sys == 'DO':
                            MPI_comp_time[f'MOCFE {link}'] = comm_time * (1 - comm_fact) / comm_fact
                        exec_time = comm_time + MPI_comp_time[f'MOCFE {link}']
                    elif app == 'BigFFT':
                        comm_fact = 0.99
                        comm_time = 0.881 * data[f'alltoall_10KB {sys} {link}']['exec_time']\
                            + 0.119 * data[f'barrier {sys} {link}']['exec_time']
                        if sys != 'WB':
                            traffic = 1 * data[f'alltoall_10KB {sys} {link}']['wt_traffic']
                            if sys == 'MP':
                                traffic *= 6/7
                        else:
                            traffic = 1 * (data[f'alltoall_10KB WB {link}']['all_traffic'] - data[f'alltoall_10KB DO {link}']['all_traffic'] + data[f'alltoall_10KB DO {link}']['wt_traffic'])
                        if sys == 'DO':
                            MPI_comp_time[f'BigFFT {link}'] = comm_time * (1 - comm_fact) / comm_fact
                        exec_time = comm_time + MPI_comp_time[f'BigFFT {link}']
                    elif app == 'CR':
                        comm_fact = 0.63
                        comm_time = 1 * data[f'sendrecv_512B {sys} {link}']['exec_time']
                        if sys != 'WB':
                            traffic = 1 * data[f'sendrecv_512B {sys} {link}']['wt_traffic'] / 2
                            if sys == 'MP':
                                traffic *= 6/7
                        else:
                            traffic = 1 * (data[f'sendrecv_512B WB {link}']['all_traffic'] - data[f'sendrecv_512B DO {link}']['all_traffic'] + data[f'sendrecv_512B DO {link}']['wt_traffic']) / 2
                        if sys == 'DO':
                            MPI_comp_time[f'CR {link}'] = comm_time * (1 - comm_fact) / comm_fact
                        exec_time = comm_time + MPI_comp_time[f'CR {link}']
                row += [exec_time, traffic]
            rows.append(row)
        
    norm_rows = []
    for row in rows:
        norm_row = [f'{row[0]} norm', ]
        for i in range(1, len(row)):
            if i % 2 == 1:
                norm_row.append(row[i] / row[1])
            else:
                norm_row.append(row[i] / row[2])
        norm_rows.append(norm_row)
    
    rows += norm_rows
        
    measures = ['performance', 'traffic']
    cols = [f'{sys} {measure}' for sys, measure in list(itertools.product(syss, measures))]
    
    df = pd.DataFrame(rows, columns=['Applications'] + cols)
    df.to_csv('../plots/macro_perf_traffic_tso.csv', index=False)