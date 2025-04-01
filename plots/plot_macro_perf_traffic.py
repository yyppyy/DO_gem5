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

fig_size_2sub1 = (12, 2.8)
fig_size_2sub2 = (12, 2.8)

def plot_macro_common(ax_low, ax_high, postfix, link, low_ax_thres, high_ax_thres):
    syss = ['MP', 'DO', 'SO', 'WB']
    apps = ['PR', 'SSSP', 'PAD', 'TQH', 'HSTI', 'TRNS', 'MOCFE', 'CMC-2D', 'BigFFT', 'CR']
    data = pd.read_csv("macro_perf_traffic.csv")
        
    # xtick_labels = [c.split()[0] for c in data['Applications'].tolist() if 'norm' not in c and link in c]
    # assert(len(xtick_labels) == len(apps))
    data.set_index('Applications', inplace=True)
    # print(xtick_labels)
    
    # TQH cannot run with MP, set to 0
    data.loc[f'TQH CXL norm', 'MP performance'] = 0
    data.loc[f'TQH CXL norm', 'MP traffic'] = 0
    data.loc[f'TQH UPI norm', 'MP performance'] = 0
    data.loc[f'TQH UPI norm', 'MP traffic'] = 0
    
    # Xs = [data['Application'] * len(syss)]
    Xs = [[_ for _ in range(len(apps))] for sys in syss]
    Ys = [[data.loc[f'{app} {link} norm', f'{sys} {postfix}'] for app in apps] for sys in syss]
    use_high_axs = [[y > high_ax_thres for y in Y] for Y in Ys]
    # for Y in Ys:
    #   for y in Y:
    #     if y >= low_ax_thres and y <= high_ax_thres:
    #       print(y)
    #     assert(y < low_ax_thres or y > high_ax_thres)
    # dummy = [[assert(y < low_ax_thres or y > high_ax_thres) for y in Y] for Y in Ys]
    
    # print(Xs)
    # print(Ys)
    # print(use_high_axs)
    
    yyp.plot_broken_bars(ax_low, ax_high, Xs, Ys, 'Applications', 'Normalized Time' if postfix == 'performance' else 'Normalized Traffic',
                  syss, use_high_axs, xtick_labels=[] if postfix == 'performance' else apps, xtick_rotate=30,
                  width=0.18,
                  low_ymin=0.8 if postfix == 'performance' else 0.6, low_ymax=low_ax_thres,
                  high_ymin=high_ax_thres, high_ymax=3.2 if postfix == 'performance' else 4.4,
                  subplot_title=link if postfix == 'performance' else None,
                  show_xlabels=False,
                  show_xticks=(postfix != 'performance'),
                  show_ylabels=(link=='CXL'), ylabel_pos=[0, 0.7],
                  show_yticks=(link=='CXL'), _yticks=[0.8, 0.9, 1, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6, 1.7] if postfix == 'performance' else [0.6, 0.7, 0.8, 0.9, 1, 1.1, 1.2, 1.3, 1.4, 1.5, 1.6],
                  ytick_font_size=13,
                  high_yticks = [2, 2.5, 3] if postfix == 'performance' else [2.0, 3.0, 4.0],
                  to_bottom=False,
                  ygrid=True,
                #   _yticks=[0, 0.5, 1, 2, 3, 4],
                  adjust_left=0.06,
                  adjust_right=0.99,
                  adjust_bottom=0.04 if postfix == 'performance' else 0.24,
                  adjust_top=0.79 if postfix == 'performance' else 0.99)
    # print(Xs)
    # return


if __name__ == '__main__':
    fig, axs = yyp.open_subplots(2, 2, fig_size_2sub1, height_ratios=[1, 3.5])
    plot_macro_common(ax_low=axs[1, 0], ax_high=axs[0, 0], postfix='performance', link='CXL', low_ax_thres=1.7, high_ax_thres=1.8)
    plot_macro_common(ax_low=axs[1, 1], ax_high=axs[0, 1], postfix='performance', link='UPI', low_ax_thres=1.7, high_ax_thres=1.8)
    yyp.set_fig_legend(fig, axs[0], font_size=14, pos=[0.327, 0.87])
    fig.subplots_adjust(wspace=0)
    # yyp.close_subplots(f'../../figures/eval_macro_performance.pdf')
    yyp.close_subplots(f'../figures/Figure 7 (top).pdf')
    
    fig, axs = yyp.open_subplots(2, 2, fig_size_2sub2, height_ratios=[1, 4.5])    
    plot_macro_common(ax_low=axs[1, 0], ax_high=axs[0, 0], postfix='traffic', link='CXL', low_ax_thres=1.6, high_ax_thres=2)
    plot_macro_common(ax_low=axs[1, 1], ax_high=axs[0, 1], postfix='traffic', link='UPI', low_ax_thres=1.6, high_ax_thres=2)
    fig.subplots_adjust(wspace=0)
    # yyp.close_subplots(f'../../figures/eval_macro_traffic.pdf')
    yyp.close_subplots(f'../figures/Figure 7 (bottom).pdf')