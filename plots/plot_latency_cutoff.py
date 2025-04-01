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

# syss = ['SO']
sys = 'SO'
measures = ['performance', 'traffic']
# links = ['25', '50', '75', '100', '125', '150', '175', '200']
links = ['50', '100', '150', '200']
rlinks = ['200', '150', '100', '50']
# legend_lines = [f'{sys} Time' for sys in syss]
# legend_bars = [f'{sys} Traffic' for sys in syss]
fig_size_3sub = (7.5, 3.7)

# colors = ['lightgray', 'gray', 'dimgray']
colors = ['#331a00', '#994d00', 'orange']
markers = ['o', '^', 's']

ymin_line = 0
ymax_line = 3.5
ymin_bar = 0
ymax_bar = 3.5
    
def get_first_value(df, row_label, column):
    result = df.loc[row_label, column]
    return result.iloc[0] if isinstance(result, pd.Series) else result    

if __name__ == "__main__":

    df = pd.read_csv("latency_cutoff.csv")
    df.set_index('config', inplace=True)
    # print(df.loc['sys SO store 4KB sync 4KB n 2 link CXL', 'performance'])

    fig = plt.figure(figsize=fig_size_3sub)
    fid = 0
    
    positions = [
        [0.095, 0.15, 0.26, 0.53],   # First subplot (leftmost)
        [0.37, 0.15, 0.26, 0.53],   # Second subplot (adjacent to first, no gap)
        [0.645, 0.15, 0.26, 0.53],  # Third subplot (some gap)
    ]
    
    # axs = [fig.add_subplot(gs[i]) for i in range(4)]
    axs = [fig.add_axes(pos) for pos in positions]
       
    # exp2_stores = ['8B', '64B', '256B', '1KB', '4KB']
    exp2_stores = ['8B', '64B', '4KB']
    exp2_syncs = ['4KB',]
    exp2_ns = ['2',]
    for sync, n in itertools.product(exp2_syncs, exp2_ns): # each is a figure
        Xs = [[int(link) / 25 for link in rlinks] for _ in range(len(exp2_stores))]
        Ylines = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[0]) for link in links] for store in exp2_stores]
        # Ylines = [[print(f'sys {sys} store {store} sync {sync} n {n} link {link}') for store in exp2_stores] for sys in syss]
        Ybars = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[1]) for link in links] for store in exp2_stores]
        # print(Ylines)
        # print(Ybars)
        yyp.plot_lines(axs[fid], Xs, Ylines, 'Inter-PU Directory Accessing Latency (ns)', 'SO Time Norm. to DO (line)',
                  [_[:-1] for _ in exp2_stores],
                  subplot_title='Store Granularity (B)', subplot_title_font_size=14, subplot_title_pad=71, subplot_title_x=0.4,
                  _ymin=ymin_line, _ymax=ymax_line,
                  xlabelpad=3,
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=True, show_yticks=True, _yticks=[0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5],
                  show_xlabels=True,
                  xtick_labels=[int(_) * 2 for _ in links], xtick_rotate=0,
                  xlabel_pos=[1.5, 0],
                  linestyles=['solid', 'solid', 'solid', 'solid', 'solid'],
                  markers=markers,
                  colors=colors,
                  markersize=5,
                  linewidth=2)
        right_ax = axs[fid].twinx()
        yyp.plot_bars(right_ax, Xs, Ybars, '', '',
                  [_[:-1] for _ in exp2_stores],
                  _ymin=ymin_bar, _ymax=ymax_bar,
                  width=0.5,
                  show_xlabels=False, show_xticks=False,
                  left_ax=axs[fid],
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=False,
                  show_yticks=False,
                  colors=colors,
                   _yticks=[0, 0.5, 1, 1.5, 2, 2.5, 3], ygrid=True,
                #   subplot_title=link,
                )
        yyp.set_ax_legend(axs[fid], ncol=2, pos=[-0.05, 1.04], handletextpad=0.3, font_size=13, right_ax=right_ax)
        fid += 1
        
    exp1_stores = ['64B',]
    # exp1_syncs = ['64B', '512B', '4KB', '32KB', '256KB', '2MB']
    exp1_syncs = ['64B', '4KB', '256KB']
    exp1_ns = ['2',]
    for store, n, in itertools.product(exp1_stores, exp1_ns): # each is a figure
        Xs = [[int(link) / 25 for link in rlinks] for _ in range(len(exp1_syncs))]
        Ylines = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[0]) for link in links] for sync in exp1_syncs]
        Ybars = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[1]) for link in links] for sync in exp1_syncs]
        yyp.plot_lines(axs[fid], Xs, Ylines, None, '',
                  [_[:-1] for _ in exp1_syncs],
                  subplot_title='Sync. Granularity (B)', subplot_title_font_size=14, subplot_title_pad=71, subplot_title_x=0.5,
                  _ymin=ymin_line, _ymax=ymax_line,
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=False,
                  show_xlabels=True,
                  xtick_labels=[int(_) * 2 for _ in links], xtick_rotate=0,
                  xlabel_pos=[0.95, 0],
                  linestyles=['solid', 'solid', 'solid', 'solid', 'solid'],
                  markers=markers,
                  colors=colors,
                  markersize=5,
                  linewidth=2)
        right_ax = axs[fid].twinx()
        yyp.plot_bars(right_ax, Xs, Ybars, '', '',
                  [_[:-1] for _ in exp1_syncs],
                  _ymin=ymin_bar, _ymax=ymax_bar,
                  width=0.5,
                  show_xlabels=False, show_xticks=False,
                  left_ax=axs[fid],
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=False,
                  show_yticks=False,
                  colors=colors,
                   _yticks=[0, 0.5, 1, 1.5, 2, 2.5, 3], ygrid=True,
                # xlabel_pos=[1, 2],
                #   subplot_title=link,
                )
        yyp.set_ax_legend(axs[fid], ncol=2, pos=[-0.04, 1.04], handletextpad=0.3, font_size=13, right_ax=right_ax)
        fid += 1

    exp3_stores = ['64B',]
    exp3_syncs = ['64B',]
    exp3_ns = ['2', '4', '8']
    for store, sync in itertools.product(exp3_stores, exp3_syncs): # each is a figure
        Xs = [[int(link) / 25 for link in rlinks] for _ in range(len(exp3_ns))]
        Ylines = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[0]) for link in links] for n in exp3_ns]
        Ybars = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measures[1]) for link in links] for n in exp3_ns]
        yyp.plot_lines(axs[fid], Xs, Ylines, None, '',
                  [int(_)-1 for _ in exp3_ns],
                  subplot_title='Comm. Fanout (# PUs)', subplot_title_font_size=14, subplot_title_pad=71, subplot_title_x=0.7,
                  _ymin=ymin_line, _ymax=ymax_line,
                  xlabelpad=28,
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                  show_ylabels=False,
                  show_xlabels=True,
                  xtick_labels=[int(_) * 2 for _ in links], xtick_rotate=0,
                  xlabel_pos=[0.95, 0],
                #   subplot_title=link,
                  linestyles=['solid', 'solid', 'solid'],
                  markers=markers,
                  colors=colors,
                  markersize=5,
                  linewidth=2)
        right_ax = axs[fid].twinx()
        yyp.plot_bars(right_ax, Xs, Ybars, '', 'SO Traffic Norm. to DO (bar)',
                  [int(_)-1 for _ in exp3_ns],
                  _ymin=ymin_bar, _ymax=ymax_bar,
                  width=0.5,
                  left_ax=axs[fid],
                  show_xlabels=False, show_xticks=False,
                  show_ylabels=True, show_yticks=True, _yticks=[0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5], ygrid=True,
                  colors=colors,
                  )
                #   xpos=[1, 9],
                #   ytick_pad=0.5, ylabel_coords=(-0.45, 0.5),
                #   log_scale=(component == 'dir'), sci_notation=(component == 'dir'),
                #   subplot_title=link,
        yyp.set_ax_legend(axs[fid], ncol=2, pos=[0.22, 1.04], handletextpad=0.3, font_size=13, right_ax=right_ax)
        fid += 1
            
    # fig.subplots_adjust(wspace=0)
    # yyp.set_fig_legend(fig, axs, right_ax=last_right_ax, ncol=6, handletextpad=0.3, font_size=11)
    # yyp.close_subplots(f'../../figures/eval_latency_cutoff.pdf')
    yyp.close_subplots(f'../figures/Figure 9.pdf')
    
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