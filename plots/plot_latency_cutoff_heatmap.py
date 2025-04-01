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
import seaborn as sns

syss = ['MP', 'SO']
do = 'DO'
measures = ['performance', 'traffic']
links = ['25', '50', '75', '100', '125', '150', '175', '200']
legend_lines = [f'{sys} Time' for sys in syss]
legend_bars = [f'{sys} Traffic' for sys in syss]
fig_size_16sub = (20, 20)
    
def get_first_value(df, row_label, column):
    result = df.loc[row_label, column]
    return result.iloc[0] if isinstance(result, pd.Series) else result    

if __name__ == "__main__":

    df = pd.read_csv("latency_cutoff.csv")
    df.set_index('config', inplace=True)
    # print(df.loc['sys SO store 4KB sync 4KB n 2 link CXL', 'performance'])

    # fig = plt.figure(figsize=fig_size_6sub)
    dimx = 4
    dimy = 4
    fig, axs = yyp.open_subplots(dimx, dimy, fig_size_16sub)
    fid = 0
    
    # positions = [
    #     [0.05, 0.27, 0.17, 0.50],   # First subplot (leftmost)
    #     [0.22, 0.27, 0.17, 0.50],   # Second subplot (adjacent to first, no gap)
    #     [0.41, 0.27, 0.17, 0.50],  # Third subplot (some gap)
    #     [0.58, 0.27, 0.17, 0.50],   # Fourth subplot (some gap)
    #     [0.77, 0.27, 0.09, 0.50],   # Fourth subplot (some gap)
    #     [0.86, 0.27, 0.09, 0.50]   # Fourth subplot (some gap)
    # ]
    
    # axs = [fig.add_subplot(gs[i]) for i in range(4)]
    # axs = [fig.add_axes(pos) for pos in positions]

    exp2_stores = ['8B', '64B', '256B', '1KB', '4KB']
    exp2_syncs = ['4KB',]
    exp2_ns = ['2',]
    
    exp1_stores = ['64B',]
    # exp1_syncs = ['64B', '512B', '4KB', '32KB', '256KB', '2MB']
    exp1_syncs = ['64B', '512B', '4KB', '32KB', '256KB']
    exp1_ns = ['2',]
    
    exp3_stores = ['64B',]
    exp3_syncs = ['64B']
    exp3_ns = ['2', '4', '8']
    
    exp4_stores = ['64B',]
    exp4_syncs = ['4KB']
    exp4_ns = ['2', '4', '8']

    all_data = []
    for sys in syss:
      for measure in measures:
        for sync, n, link in itertools.product(exp2_syncs, exp2_ns, links): # each is a figure
            all_data += [get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measure) for store in exp2_stores]
        for store, n, link in itertools.product(exp1_stores, exp1_ns, links): # each is a figure
            all_data += [get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measure) for sync in exp1_syncs]
        for sync, store, link in itertools.product(exp3_syncs, exp3_stores, links): # each is a figure
            all_data += [get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measure) for n in exp3_ns]
        for sync, store, link in itertools.product(exp4_syncs, exp4_stores, links): # each is a figure
            all_data += [get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measure) for n in exp4_ns]
    all_data = np.array(all_data)
    print(all_data)
    global_min = np.min(all_data)
    global_max = np.max(all_data)
    print(f"min:{global_min}, max:{global_max}")

    cmap = sns.color_palette("coolwarm", as_cmap=True)
    norm = plt.Normalize(vmin=global_min, vmax=global_max)
    
    for sys in syss:
      for measure in measures:
        for sync, n in itertools.product(exp2_syncs, exp2_ns): # each is a figure
            data = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measure) for store in exp2_stores] for link in links]
            sns.heatmap(data, annot=True, fmt=".2f", cmap="coolwarm", linewidths=0.5, ax=axs[fid // dimx, fid % dimx],
                        vmin=global_min, vmax=global_max, center=1.0, cbar=False)
            axs[fid // dimx, fid % dimx].set_title(f'sys {sys}, store, n {n}, measure {measure}')
            fid += 1
        for store, n in itertools.product(exp1_stores, exp1_ns): # each is a figure
            data = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measure) for sync in exp1_syncs] for link in links]
            sns.heatmap(data, annot=True, fmt=".2f", cmap="coolwarm", linewidths=0.5, ax=axs[fid // dimx, fid % dimx],
                        vmin=global_min, vmax=global_max, center=1.0, cbar=False)
            axs[fid // dimx, fid % dimx].set_title(f'sys {sys}, sync, n {n}, measure {measure}')
            fid += 1
        for sync, store in itertools.product(exp3_syncs, exp3_stores): # each is a figure
            data = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measure) for n in exp3_ns] for link in links]
            sns.heatmap(data, annot=True, fmt=".2f", cmap="coolwarm", linewidths=0.5, ax=axs[fid // dimx, fid % dimx],
                        vmin=global_min, vmax=global_max, center=1.0, cbar=False)
            axs[fid // dimx, fid % dimx].set_title(f'sys {sys}, fanout, n {n}, measure {measure}')
            fid += 1
        for sync, store, in itertools.product(exp4_syncs, exp4_stores): # each is a figure
            data = [[get_first_value(df, f'sys {sys} store {store} sync {sync} n {n} link {link}', measure) for n in exp4_ns] for link in links]
            sns.heatmap(data, annot=True, fmt=".2f", cmap="coolwarm", linewidths=0.5, ax=axs[fid // dimx, fid % dimx],
                        vmin=global_min, vmax=global_max, center=1.0, cbar=False)
            axs[fid // dimx, fid % dimx].set_title(f'sys {sys}, fanout, n {n}, measure {measure}')
            fid += 1
    
    cbar_ax = fig.add_axes([0.25, 0.94, 0.5, 0.02])  # [left, bottom, width, height]
    sm = plt.cm.ScalarMappable(cmap=cmap, norm=norm)
    fig.colorbar(sm, cax=cbar_ax, orientation='horizontal', label="Heatmap Value Scale")
    plt.tight_layout(rect=[0, 0, 1, 0.92])  # Adjust layout to fit colorbar
            
    # fig.subplots_adjust(wspace=0)
    # yyp.set_fig_legend(fig, axs, right_ax=last_right_ax, ncol=6, handletextpad=0.3, font_size=14)
    yyp.close_subplots(f'../../figures/eval_latency_cutoff.pdf')
    
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