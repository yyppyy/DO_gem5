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

if __name__ == "__main__":
        
    syss = ['DO', 'SO', 'MP']
    measures = ['performance', 'traffic']
    
    configs = []
    links = ['25', '50', '75', '100', '125', '150', '175', '200']
    
    # the below order of three exps should align with the order in run_latency_cutoff.sh
    exp1_stores = ['64B',]
    exp1_syncs = ['64B', '512B', '4KB', '32KB', '256KB']
    exp1_ns = ['2',]
    configs += list(itertools.product(exp1_stores, exp1_syncs, exp1_ns, syss, links))
    
    exp3_stores = ['64B',]
    exp3_syncs = ['64B', '4KB']
    exp3_ns = ['2', '4', '8']
    configs += list(itertools.product(exp3_stores, exp3_syncs, exp3_ns, syss, links))
    
    exp2_stores = ['8B', '64B', '256B', '1KB', '4KB']
    exp2_syncs = ['4KB',]
    exp2_ns = ['2',]
    configs += list(itertools.product(exp2_stores, exp2_syncs, exp2_ns, syss, links))
    
    data = []
    file_idx = 500
    perf_norm_dict = {}
    traffic_norm_dict = {}
    for store, sync, n, sys, link in configs:
        row = [f'sys {sys} store {store} sync {sync} n {n} link {link}', ]
        for measure in measures:
            if measure == 'performance':
                time = pp.process_logfile_comp_time_lat_cutoff(logfile=f'../results/logs/log{file_idx}.txt', latency=link)
                if sys == 'DO':
                    perf_norm_dict[f'sys DO store {store} sync {sync} n {n} link {link}'] = time
                time /= perf_norm_dict[f'sys DO store {store} sync {sync} n {n} link {link}']
                row.append(time)
            else:
                _, traffic = pp.find_flits_count(statsfile=f'../results/m5out/stats{file_idx}.txt', flit_size=16)
                if sys == 'DO':
                    traffic_norm_dict[f'sys DO store {store} sync {sync} n {n} link {link}'] = traffic
                traffic /= traffic_norm_dict[f'sys DO store {store} sync {sync} n {n} link {link}']
                row.append(traffic)
        file_idx += 1
        data.append(row)

    df = pd.DataFrame(data, columns=['config'] + measures)
    df.to_csv('../plots/latency_cutoff.csv', index=False)