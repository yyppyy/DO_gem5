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

import itertools
import pandas as pd
import yyplot as yyp
import matplotlib.pyplot as plt
import numpy as np

syss = ['MP', 'DO', 'SO']
measures = ['performance', 'traffic']
links = ['CXL', 'UPI']
legend_lines = [f'{sys} Time' for sys in syss]
legend_bars = [f'{sys} Traffic' for sys in syss]
fig_size_6sub = (12, 3.4)
    
def get_first_value(df, row_label, column):
    result = df.loc[row_label, column]
    return result.iloc[0] if isinstance(result, pd.Series) else result    

if __name__ == "__main__":

    df = pd.read_csv("sensitivity_analysis.csv")
    df.set_index('config', inplace=True)
    # print(df.loc['sys SO store 4KB sync 4KB n 2 link CXL', 'performance'])

    fig = plt.figure(figsize=fig_size_6sub)
    fid = 0
    
    positions = [
        [0.05, 0.27, 0.17, 0.50],   # First subplot (leftmost)
        [0.22, 0.27, 0.17, 0.50],   # Second subplot (adjacent to first, no gap)
        [0.41, 0.27, 0.17, 0.50],  # Third subplot (some gap)
        [0.58, 0.27, 0.17, 0.50],   # Fourth subplot (some gap)
        [0.77, 0.27, 0.09, 0.50],   # Fourth subplot (some gap)
        [0.86, 0.27, 0.09, 0.50]   # Fourth subplot (some gap)
    ]
    
    # axs = [fig.add_subplot(gs[i]) for i in range(4)]
    axs = [fig.add_axes(pos) for pos in positions]
       
    exp2_stores = ['8B', '64B', '256B', '1KB', '4KB']
    exp2_syncs = ['4KB',]
    exp2_ns = ['2',]
    for sync, n, link in itertools.product(exp2_syncs, exp2_ns, links): # each is a figure
        Xs = [[1, 3, 4, 5, 6] for _ in range(len(syss))]
        Ylines = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[0]) for store in exp2_stores] for sys in syss]
        # Ylines = [[print(f'sys {sys} store {store} sync {sync} n {n} link {link}') for store in exp2_stores] for sys in syss]
        Ybars = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[1]) for store in exp2_stores] for sys in syss]
        # print(Ylines)
        # print(Ybars)
        yyp.plot_lines(axs[fid], Xs, Ylines, 'Store Granularity (B)', 'Normalized Time',
                  legend_lines,
                  subplot_title=link,
                  _ymin=0, _ymax=3.5,
                  xlabelpad=12,
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=(link == links[0]), show_yticks=(link == links[0]), _yticks=[0, 1, 2, 3],
                  show_xlabels=(link == links[0]),
                  xtick_labels=[s[:-1] for s in exp2_stores], xtick_rotate=45,
                  xlabel_pos=[0.95, 0],
                  linestyles=['solid', 'dotted', '--', 'dashdot'],
                  markers=['s', 's', 's', 's'],
                  # markersize=3,
                  linewidth=2)
        yyp.plot_bars(axs[fid].twinx(), Xs, Ybars, '', '',
                  legend_bars,
                  _ymin=0, _ymax=2.2,
                  width=0.25,
                  show_xlabels=False, show_xticks=False,
                  left_ax=axs[fid],
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=False,
                #   subplot_title=link,
                )
        fid += 1
        
    exp1_stores = ['64B',]
    exp1_syncs = ['64B', '512B', '4KB', '32KB', '256KB', '2MB']
    exp1_ns = ['2',]
    for store, n, link in itertools.product(exp1_stores, exp1_ns, links): # each is a figure
        Xs = [[1, 2, 3, 4, 5, 6] for _ in range(len(syss))]
        Ylines = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[0]) for sync in exp1_syncs] for sys in syss]
        Ybars = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[1]) for sync in exp1_syncs] for sys in syss]
        yyp.plot_lines(axs[fid], Xs, Ylines, 'Sync Granularity (B)', '',
                  legend_lines,
                  subplot_title=link,
                  _ymin=0, _ymax=3.5,
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=False,
                  show_xlabels=(link == links[0]),
                  xtick_labels=[s[:-1] for s in exp1_syncs], xtick_rotate=45,
                  xlabel_pos=[0.95, 0],
                  linestyles=['solid', 'dotted', '--', 'dashdot'],
                  markers=['s', 's', 's', 's'],
                  # markersize=3,
                  linewidth=2)
        yyp.plot_bars(axs[fid].twinx(), Xs, Ybars, '', '',
                  legend_bars,
                  _ymin=0, _ymax=2.2,
                  width=0.25,
                  show_xlabels=False, show_xticks=False,
                  left_ax=axs[fid],
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=False,
                # xlabel_pos=[1, 2],
                #   subplot_title=link,
                )
        fid += 1

    exp3_stores = ['64B',]
    exp3_syncs = ['4KB',]
    exp3_ns = ['2', '4', '8']
    for store, n, link in itertools.product(exp3_stores, exp3_stores, links): # each is a figure
        Xs = [[1, 2, 3] for _ in range(len(syss))]
        Ylines = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[0]) for n in exp3_ns] for sys in syss]
        Ybars = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[1]) for n in exp3_ns] for sys in syss]
        yyp.plot_lines(axs[fid], Xs, Ylines, 'Communication Fanout (# PUs)', '',
                  legend_lines,
                  subplot_title=link,
                  _ymin=0, _ymax=3.5,
                  xlabelpad=28,
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=False,
                  show_xlabels=(link == links[0]),
                  xtick_labels=[int(n) - 1 for n in exp3_ns], xtick_rotate=0,
                  xlabel_pos=[0.95, 0],
                #   subplot_title=link,
                  linestyles=['solid', 'dotted', '--', 'dashdot'],
                  markers=['s', 's', 's', 's'],
                  # markersize=3,
                  linewidth=2)
        last_right_ax = axs[fid].twinx()
        yyp.plot_bars(last_right_ax, Xs, Ybars, '', 'Traffic',
                  legend_bars,
                  _ymin=0, _ymax=2.2,
                  width=0.2,
                  left_ax=axs[fid],
                  show_xlabels=False, show_xticks=False,
                  show_ylabels=(link == links[-1]), show_yticks=(link == links[-1]), _yticks=[0, 1, 2],
                  )
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                #   subplot_title=link,
        fid += 1
            
    # fig.subplots_adjust(wspace=0)
    yyp.set_fig_legend(fig, axs, right_ax=last_right_ax, ncol=6, handletextpad=0.3, font_size=14)
    # yyp.close_subplots(f'../../figures/eval_sensitivity_analysis.pdf')
    yyp.close_subplots(f'../figures/Figure 8.pdf')
    
    # exp1:
    # each sync, n, link is a figure
    # each sys perf is a line, each store is a point
    # each sys traffic is a bar, each store is a point

    # exp2:
    # each store, n, link is a figure
    # each sys perf is a line, each sync is a point
    # each sys traffic is a bar, each sync is a point

    # exp3:
    # each store, sync, link is a figure
    # each sys perf is a line, each n is a point
    # each sys traffic is a bar, each n is a point