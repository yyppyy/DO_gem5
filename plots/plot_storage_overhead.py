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

fig_size_4sub = (6, 3)

components = ['proc', 'dir']
links = ['CXL', 'UPI']

def plot_storage_percentage_common(ax, component, link):
    
    apps = ['SSSP', 'PAD', 'PR', 'ATA',]
    nodes = ['2', '4', '8']
    
    Xs = [[2, 4, 8] for _ in range(len(apps))]
    
    data = pd.read_csv("storage_overhead.csv")
    
    Ys = [[data.loc[data.iloc[:, 0] == f'{app} {node} abs', f'{component}_total {link}'].values[0] for node in nodes] for app in apps]
    
    yyp.plot_lines(ax, Xs, Ys, 'Number of PUs', f"{'Proc' if component == 'proc' else 'Dir'} Storage (B)",
                  apps, _yticks=[0, 500, 1000, 1500] if component == 'dir' else [0, 10, 20, 30, 40],
                  _ymin=0, _ymax=1600 if component == 'dir' else 40,
                  xpos=[1, 9],
                  ytick_pad=0.5, ylabel_coords=(-0.35, 0.5) if (component == components[0]) else (-0.58, 0.5),
                  log_scale=False, sci_notation=False,
                  show_ylabels=(link == links[0]), show_yticks=(link == links[0]),
                  show_xlabels=(link == links[0]), xlabel_pos=[1, 2],
                  subplot_title=link,
                  colors=['black', 'orange', 'green', 'blue'],
                #   colors=['#1E90FF', '#ADD8E6', '#87CEEB', '#4169E1'],
                  linestyles=['dashdot', '--', 'dotted', 'solid'],
                  markers=['s', 's', 's', 's'],
                  linewidth=2)
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
        [0.10, 0.19, 0.18, 0.53],   # First subplot (leftmost)
        [0.28, 0.19, 0.18, 0.53],   # Second subplot (adjacent to first, no gap)
        [0.63, 0.19, 0.18, 0.53],  # Third subplot (some gap)
        [0.81, 0.19, 0.18, 0.53]   # Fourth subplot (some gap)
    ]
    
    # axs = [fig.add_subplot(gs[i]) for i in range(4)]
    axs = [fig.add_axes(pos) for pos in positions]
    
    fid = 0
    for component in components:
        for link in links:
            plot_storage_percentage_common(axs[fid], component, link)
            fid += 1
            
    fig.subplots_adjust(wspace=0)
    yyp.set_fig_legend(fig, axs)
    # yyp.close_subplots(f'../../figures/eval_storage_percentage.pdf')
    yyp.close_subplots(f'../figures/Figure 11.pdf')