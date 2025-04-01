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

def calc_perf_traffic(perf_base, traffic_base, rt, n_rlx_st, n_rel_st, flit_size, store_size, n_epoch=0, n_counter=0, n_seq=0):
    if n_seq != 0:
        perf = perf_base + (n_rlx_st + n_rel_st) / n_seq * rt
        traffic = traffic_base + n_rlx_st * (store + (2 if n_seq > 256 else 1) * flit_size) + n_rel_st * 2 * flit_size
    else:
        assert(n_epoch != 0 and n_counter != 0)
        perf = perf_base + n_rlx_st / n_counter * rt + n_rel_st / n_epoch * rt
        traffic = traffic_base +  n_rlx_st * (store + (2 if n_epoch > 256 else 1) * flit_size) + n_rel_st * 2 * flit_size
    return perf, traffic

if __name__ == "__main__":
        
    flit_size = 16
    store = 64
    rel_size = flit_size
    sync = 1024 * 1024 * 2
    r = 7
    n_rlx_st = sync / store * 7
    n_rel_st = r
    link_to_rt = {'CXL':0.3, 'UPI':0.1}
    
    link_to_perf_base = {}
    link_to_perf_base['CXL'] = pp.process_logfile_comp_time_stnt_ldnt(logfile=f'../results/logs/log31.txt', r=1)
    link_to_perf_base['UPI'] = pp.process_logfile_comp_time_stnt_ldnt(logfile=f'../results/logs/log32.txt', r=1)
    
    configs = ['SEQ 8 CXL', 'DO 8 8 CXL', 'DO 8 16 CXL', 'DO 8 32 CXL', 'SEQ 40 CXL',
               'DO 4 32 CXL', 'DO 16 32 CXL',
               'SEQ 8 UPI', 'DO 8 8 UPI', 'DO 8 16 UPI', 'DO 8 32 UPI', 'SEQ 40 UPI',
               'DO 4 32 UPI', 'DO 16 32 UPI',]
    
    data_map = {}
    link_to_perf_norm = {}
    link_to_traffic_norm = {}
    for config in configs:
        cfg = config.split()
        link = cfg[-1]
        if (cfg[0] == 'SEQ'):
            perf, traffic = calc_perf_traffic(perf_base=link_to_perf_base[link], traffic_base=0, rt=link_to_rt[link],
                              n_rlx_st=n_rlx_st, n_rel_st=n_rel_st, flit_size=flit_size,
                              store_size=store, n_seq=2**int(cfg[1]))
            if cfg[1] == '40':
                link_to_perf_norm[link] = perf
            elif cfg[1] == '8':
                link_to_traffic_norm[link] = traffic
            data_map[config] = (perf, traffic)
        else:
            perf, traffic = calc_perf_traffic(perf_base=link_to_perf_base[link], traffic_base=0, rt=link_to_rt[link],
                              n_rlx_st=n_rlx_st, n_rel_st=n_rel_st, flit_size=flit_size,
                              store_size=store, n_epoch=2**int(cfg[1]), n_counter=2**int(cfg[2]))
            data_map[config] = (perf, traffic)

    data = [[config,
             data_map[config][0] / link_to_perf_norm[config.split()[-1]],
             data_map[config][1] / link_to_traffic_norm[config.split()[-1]],
             ] for config in configs]
            
    df = pd.DataFrame(data, columns=['config', 'performance', 'traffic'])
    df.to_csv('../plots/seq_no.csv', index=False)