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
legend_lines = ['Normalized Time',]
legend_bars = ['Traffic',]
fig_size_4sub = (6, 3)
    
def get_first_value(df, row_label, column):
    result = df.loc[row_label, column]
    return result.iloc[0] if isinstance(result, pd.Series) else result    

if __name__ == "__main__":

    df = pd.read_csv("seq_no.csv")
    df.set_index('config', inplace=True)
    # print(df.loc['sys SO store 4KB sync 4KB n 2 link CXL', 'performance'])

    fig = plt.figure(figsize=fig_size_4sub)
    fid = 0
    
    positions = [
        [0.12, 0.17, 0.18, 0.50],   # First subplot (leftmost)
        [0.3, 0.17, 0.18, 0.50],   # Second subplot (adjacent to first, no gap)
        [0.52, 0.17, 0.18, 0.50],  # Third subplot (some gap)
        [0.70, 0.17, 0.18, 0.50],   # Fourth subplot (some gap)
    ]
    
    # axs = [fig.add_subplot(gs[i]) for i in range(4)]
    axs = [fig.add_axes(pos) for pos in positions]
    right_axs = []
    # exp1_configs = {}
    # exp1_configs['CXL'] = ['SEQ 8 CXL', 'DO 8 8 CXL', 'DO 8 16 CXL', 'DO 8 32 CXL', 'SEQ 40 CXL']
    # exp1_configs['UPI'] = ['SEQ 8 UPI', 'DO 8 8 UPI', 'DO 8 16 UPI', 'DO 8 32 UPI', 'SEQ 40 UPI']
    # exp2_configs = {}
    # exp2_configs['CXL'] = ['SEQ 8 CXL', 'DO 4 32 CXL', 'DO 8 32 CXL', 'DO 16 32 CXL', 'SEQ 40 CXL']
    # exp2_configs['UPI'] = ['SEQ 8 UPI', 'DO 4 32 UPI', 'DO 8 32 UPI', 'DO 16 32 UPI', 'SEQ 40 UPI']
    
    fid = 0
    for rep in range(2):
      for link in links:
        Xs = [[1, ]]
        configs = [f'SEQ 8 {link}',]
        Ylines = [[get_first_value(df, config, 'performance') for config in configs],]
        Ybars = [[get_first_value(df, config, 'traffic') for config in configs],]
        yyp.plot_lines(axs[fid], Xs, Ylines, 'Bit-width', 'Normalized Time',
                  ['SEQ-8 Time',],
                  subplot_title=link,
                  _ymin=0.75, _ymax=1.35,
                  xlabelpad=12,
                  show_ylabels=False,
                  show_xlabels=False,
                  # xtick_labels=['8'], xtick_rotate=45,
                  # xlabel_pos=[0.95, 0],
                  linestyles=['dotted',],
                  markers=['s',],
                  colors=['green',],
                  linewidth=2)
        right_ax = axs[fid].twinx()
        yyp.plot_bars(right_ax, Xs, Ybars, '', '',
                  ['SEQ-8 Traffic',],
                  _ymin=0.75, _ymax=1.25,
                  width=0.4,
                  show_xlabels=False, show_xticks=False,
                  left_ax=axs[fid],
                  show_ylabels=False, show_yticks=False,
                  colors=['green',],
                  hatchs=['/////',],
                )
        if rep == 0 and link == links[0]:
          right_axs.append(right_ax)

        Xs = [[2, 3, 4],]
        configs = [f'DO 8 8 {link}', f'DO 8 16 {link}', f'DO 8 32 {link}'] if rep == 0\
          else [f'DO 8 32 {link}', f'DO 8 32 {link}', f'DO 16 32 {link}']
        Ylines = [[get_first_value(df, config, 'performance') for config in configs],]
        Ybars = [[get_first_value(df, config, 'traffic') for config in configs],]
        yyp.plot_lines(axs[fid], Xs, Ylines, 'Bit-width', 'Normalized Time',
                  ['DO Time',],
                  subplot_title=link,
                  _ymin=0.75, _ymax=1.35,
                  xlabelpad=12,
                  show_ylabels=False,
                  show_xlabels=False,
                  xtick_labels=['8', '16', '32'] if rep == 0 else ['4', '8', '16'],
                  # xlabel_pos=[0.95, 0],
                  linestyles=['solid',],
                  markers=['s',],
                  colors=['blue',],
                  linewidth=2)
        right_ax = axs[fid].twinx()
        yyp.plot_bars(right_ax, Xs, Ybars, '', '',
                  ['DO Traffic',],
                  _ymin=0.75, _ymax=1.25,
                  width=0.4,
                  show_xlabels=False, show_xticks=False,
                  left_ax=axs[fid],
                  show_ylabels=False, show_yticks=False,
                  colors=['blue',],
                  hatchs=['\\\\\\\\\\',],
                )
        if rep == 0 and link == links[0]:
          right_axs.append(right_ax)

        Xs = [[5, ],]
        configs = [f'SEQ 40 {link}',]
        Ylines = [[get_first_value(df, config, 'performance') for config in configs],]
        Ybars = [[get_first_value(df, config, 'traffic') for config in configs],]
        yyp.plot_lines(axs[fid], Xs, Ylines, f"{'epoch' if rep == 1 else 'store counter'} bit-width", 'Normalized Time',
                  ['SEQ-40 Time',],
                  subplot_title=link,
                  _ymin=0.75, _ymax=1.35,
                  xlabelpad=0,
                  show_ylabels=(link == links[0]) and rep == 0, show_yticks=(link == links[0]) and rep == 0, _yticks=[0.8, 0.9, 1, 1.1, 1.2, 1.3],
                  show_xlabels=(link == links[0]),
                  # xtick_labels=['40'], xtick_rotate=45,
                  xlabel_pos=[0.9, 0.5] if rep == 0 else [1.1, 0],
                  linestyles=['dotted',],
                  markers=['s',],
                  colors=['orange',],
                  linewidth=2)
        right_ax = axs[fid].twinx()
        yyp.plot_bars(right_ax, Xs, Ybars, '', 'Traffic',
                  ['SEQ-40 Time',],
                  _ymin=0.75, _ymax=1.25,
                  width=0.4,
                  left_ax=axs[fid],
                  show_xlabels=False, show_xticks=False,
                  show_ylabels=(link == links[-1]) and rep == 1, show_yticks=(link == links[-1]) and rep == 1, _yticks=[0.8, 0.9, 1, 1.1, 1.2],
                  colors=['orange',],
                  hatchs=['||||||',],
                )
        if rep == 0 and link == links[0]:
          right_axs.append(right_ax)
        
        axs[fid].set_xticks([2, 3, 4])
        axs[fid].set_xticklabels(['8', '16', '32'] if rep == 0 else ['4', '8', '16'], fontsize=13)

        fid += 1

    # fig.subplots_adjust(wspace=0)
    yyp.set_fig_legend(fig, axs, other_axs=right_axs, ncol=3, handletextpad=0.3, font_size=13)
    # yyp.close_subplots(f'../../figures/eval_seq_no.pdf')
    yyp.close_subplots(f'../figures/Figure 10.pdf')