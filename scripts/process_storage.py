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
    
    sys = 'DO'
    n_nodess = ['8', '4', '2']
    links = ['CXL', 'UPI']
    apps = ['PAD', 'PR', 'SSSP', 'ATA']
    
    file_id = 85
    
    # the below order of three exps should align with the order in run_storage.sh
    data_map = {}
    for n_nodes in n_nodess:
        for link in links:
            for app in apps:
                k = f'{app} {n_nodes}'
                if k not in data_map:
                    data_map[k] = []
                proc_epoch, proc_stCnt,\
                proc_pendingEpochs,\
                dir_stCnt, dir_notiCnt,\
                dir_maxCommittedEpochs, dir_network_buffer\
                = pp.calculate_storage(f'../results/logs/log{file_id}.txt',\
                    stCnt_bw=32, epoch_bw=8, n_procs=int(n_nodes) * 8, n_dirs=int(n_nodes))
                proc_total = proc_epoch + proc_stCnt + proc_pendingEpochs
                dir_total = dir_stCnt + dir_notiCnt + dir_maxCommittedEpochs + dir_network_buffer
                
                data_map[k].append(proc_epoch)
                data_map[k].append(proc_stCnt)
                data_map[k].append(proc_pendingEpochs)
                data_map[k].append(dir_stCnt)
                data_map[k].append(dir_notiCnt)
                data_map[k].append(dir_maxCommittedEpochs)
                data_map[k].append(dir_network_buffer)
                data_map[k].append(proc_total)
                data_map[k].append(dir_total)
                
                file_id += 1
    
    data = []
    for n_nodes in n_nodess:
        for app in apps:
            row = [f'{app} {n_nodes} abs', ] + [val for val in data_map[f'{app} {n_nodes}']]
            data.append(row)
    
    measures = ['proc_epoch', 'proc_stCnt',\
        'proc_pendingEpochs', 'dir_stCnt', 'dir_notiCnt',\
        'dir_maxCommittedEpochs', 'dir_network_buffer',\
        'proc_total', 'dir_total']
    raw_cols = list(itertools.product(links, measures))
    cols = [f'{measure} {link}' for link, measure in raw_cols]
    
    df = pd.DataFrame(data, columns=['config'] + cols)
    df.to_csv('../plots/storage_overhead.csv', index=False)