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

import yyplot as yyp
import pandas as pd
from matplotlib.gridspec import GridSpec
import matplotlib.pyplot as plt
import numpy as np

fig_size_4sub = (6, 3.2)

components = ['proc', 'dir']
links = ['CXL', 'UPI']

def plot_storage_breakdown_common(ax, component, link):
    
    app = 'ATA'
    
    # breakdowns = [f'proc_{t} {link}' for t in ['epoch', 'stCnt', 'pendingEpochs']] if component == 'proc'\
    #     else [f'dir_{t} {link}' for t in ['stCnt', 'notiCnt', 'maxCommittedEpochs', 'network_buffer']]
        
    # breakdowns = ['Others', 'pendingEpochs'] if component == 'proc'\
    #     else ['Others', 'dirStRlxCnts', 'notifyCnts', 'Network Buffer']

    breakdowns = ['Store Counters', 'Other Look-up Tables'] if component == 'proc'\
        else ['Network Buffer', 'Look-up Tables']
    
    nodes = ['2', '4', '8']
    
    Xs = [[2, 4, 8] for _ in range(len(breakdowns))]
    Ys = []
    
    data = pd.read_csv("storage_overhead.csv")
    for b in breakdowns:
        if component == 'proc':
            if b == 'Other Look-up Tables':
                Y = np.array([data.loc[data.iloc[:, 0] == f'{app} {node} abs', f'proc_epoch {link}'].values[0] for node in nodes])
                Y += np.array([data.loc[data.iloc[:, 0] == f'{app} {node} abs', f'proc_pendingEpochs {link}'].values[0] for node in nodes])
            elif b == 'Store Counters':
                Y = np.array([data.loc[data.iloc[:, 0] == f'{app} {node} abs', f'proc_stCnt {link}'].values[0] for node in nodes])
        else:
            if b == 'Look-up Tables':
                Y = np.array([data.loc[data.iloc[:, 0] == f'{app} {node} abs', f'dir_maxCommittedEpochs {link}'].values[0] for node in nodes])
            # elif b == 'dirStRlxCnts':
                Y += np.array([data.loc[data.iloc[:, 0] == f'{app} {node} abs', f'dir_stCnt {link}'].values[0] for node in nodes])
            # elif b == 'notifyCnts':
                Y += np.array([data.loc[data.iloc[:, 0] == f'{app} {node} abs', f'dir_notiCnt {link}'].values[0] for node in nodes])
            elif b == 'Network Buffer':
                Y = np.array([data.loc[data.iloc[:, 0] == f'{app} {node} abs', f'dir_network_buffer {link}'].values[0] for node in nodes])
            # Y = Y.astype(float) / 1024 # to KB
        Ys.append(Y)
        
    yyp.plot_stacked_bars(ax, Xs, Ys, 'Number of PUs', f"{'Proc' if component == 'proc' else 'Dir'} Storage (B)",
                  breakdowns, _yticks=[0, 500, 1000, 1500] if component == 'dir' else [0, 10, 20, 30, 40],
                  _ymin=0, _ymax=1600 if component == 'dir' else 40,
                  xpos=[1, 9],
                  show_legend=(link == links[0]), legend_loc='upper center', bbox_to_anchor=(0.9, 1.7),
                  ytick_pad=0.5, ylabel_coords=(-0.35, 0.5) if (component == components[0]) else (-0.58, 0.5),
                  width=1,
                  log_scale=False, sci_notation=False,
                  show_ylabels=(link == links[0]), show_yticks=(link == links[0]),
                  show_xlabels=(link == links[0]), xlabel_pos=[1, 2],
                  subplot_title=link,
                #   colors=['blue', 'green'])
                  colors=['blue', 'orange'])
                #   colors=['#87CEEB', '#ADD8E6', '#1E90FF', '#4169E1'])
                #   adjust_left=0.13, adjust_right=0.99, adjust_bottom=0.19, adjust_top=0.72)

if __name__ == '__main__':
    fig = plt.figure(figsize=fig_size_4sub)
    # gs = GridSpec(1, len(components) * len(links), figure=fig, wspace=0)
    # gs.update(wspace=0.1)
    
    positions = [
        # [0.13, 0.19, 0.19, 0.53],   # First subplot (leftmost)
        # [0.32, 0.19, 0.19, 0.53],   # Second subplot (adjacent to first, no gap)
        # [0.61, 0.19, 0.19, 0.53],  # Third subplot (some gap)
        # [0.8, 0.19, 0.19, 0.53]   # Fourth subplot (some gap)
        [0.10, 0.17, 0.18, 0.50],   # First subplot (leftmost)
        [0.28, 0.17, 0.18, 0.50],   # Second subplot (adjacent to first, no gap)
        [0.63, 0.17, 0.18, 0.50],  # Third subplot (some gap)
        [0.81, 0.17, 0.18, 0.50]   # Fourth subplot (some gap)
    ]
    
    # axs = [fig.add_subplot(gs[i]) for i in range(4)]
    axs = [fig.add_axes(pos) for pos in positions]
    
    fid = 0
    for component in components:
        for link in links:
            plot_storage_breakdown_common(axs[fid], component, link)
            fid += 1
            
    fig.subplots_adjust(wspace=0)
    # yyp.set_fig_legend(fig, axs, right_ax=axs[-1], ncol=3)
    # yyp.close_subplots(f'../../figures/eval_storage_breakdown.pdf')
    yyp.close_subplots(f'../figures/Figure 12.pdf')