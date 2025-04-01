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

fig_size = (6.5, 3)

def plot_so_perf_traffic_overhead(ax, link):
    # legends = ['Latency overhead', 'Traffic overhead']
    measures = ['performance', 'traffic']
    legends = ['Execution Time', 'Traffic']
    
    apps = ['PR', 'SSSP', 'PAD', 'TQH', 'MOCFE', 'CMC-2D', 'BigFFT', 'CR']    
    data = pd.read_csv("macro_perf_traffic.csv")
    data.set_index('Applications', inplace=True)

    Xs = [[_ for _ in range(len(apps))] for m in measures]
    Ys = [[(1 - (data.loc[f'{app} {link} norm', f'MP {m}'] / data.loc[f'{app} {link} norm', f'SO {m}'])) * 100\
        for app in apps] for m in measures]
    
    yyp.plot_bars(ax, Xs, Ys, 'Applications', 'percentage (%)',
                  legends=legends, xtick_labels=apps, xtick_rotate=45,
                  width=0.35, _yticks=[0, 5, 10, 15, 20, 25, 30, 35, 40],
                  adjust_left=0.1, adjust_right=0.99, adjust_bottom=0.27, adjust_top=0.77,
                  to_bottom=False,
                  _ymin=0, _ymax=40,
                  show_ylabels=link=='CXL',
                  show_yticks=link=='CXL',
                  ygrid=True,
                  subplot_title=link)        

if __name__ == '__main__':
    fig, axs = yyp.open_subplots(1, 2, fig_size)
    plot_so_perf_traffic_overhead(ax=axs[0], link='CXL')
    plot_so_perf_traffic_overhead(ax=axs[1], link='UPI')
    yyp.set_fig_legend(fig, axs, font_size=13, pos=[0.25, 0.88])
    fig.subplots_adjust(wspace=0)    
    # yyp.close_subplots(f'../../figures/motivation_so_perf_traffic_overhead.pdf')
    yyp.close_subplots(f'../figures/Figure 2.pdf')