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

import matplotlib
matplotlib.use('Agg')
from matplotlib import pyplot as plt
from matplotlib.ticker import ScalarFormatter
import matplotlib.ticker as mticker
import matplotlib as mpl
import numpy as np
font_size = 15
mpl.rcParams['font.size'] = font_size
plt.ioff()

sys2color = {'gcp': '#B22222', 'soul': '#B22222', 'soul_user': '#B22222', 'ideal': 'orange', 'cohort_soul_pthread': '#B22222', 'spinlock': '#FFB3B3', 'mcs': '#008080', 'cohort_rw_spin_mutex': 'orange', 'pthread_rwlock_prefer_w': '#87CEEB', 'percpu': '#4169e1', 'soul_wo_o_opt': '#4169e1', 'soul_wo_locality_opt': '#FFB3B3', 'soul_wo_combined_data_opt': '#1E90FF', 'lock_service': '#FFB3B3', 'qosb': '#FFB3B3'}
sys2hatch = {'gcp': '-------', 'soul': '', 'soul_user': '-------', 'ideal': '', 'spinlock': '///', 'mcs': '\\\\\\', 'pthread_rwlock_prefer_w': '++', 'percpu': 'xxx', 'cohort_rw_spin_mutex': '', 'lock_service': '....', 'qosb': '....'}
sys2markers = {'gcp': 'o', 'soul': 'o', 'soul_user': 'o', 'ideal': '>', 'cohort_soul_pthread': 'o', 'spinlock': '^', 'mcs': 'x', 'pthread_rwlock_prefer_w': 's', 'percpu': '+', 'cohort_rw_spin_mutex': '<', 'cohort_spin_mutex': '>', 'soul_wo_o_opt': '+', 'soul_wo_locality_opt': '<', 'soul_wo_combined_data_opt': '>',  'lock_service': '>', 'qosb': '>'}
sys2lstyle = {'gcp': '-', 'soul': '-', 'soul_user': '-', 'soul_wo_o_opt': 'dashdot', 'soul_wo_locality_opt': '--', 'soul_wo_combined_data_opt': ':'}
colors = ['#B22222', 'green', 'blue', 'orange', '#FFB3B3', 'black', 'turquoise']
# colors = ['#B22222', '#B22222', '#B22222', '#B22222', 'blue', 'blue', 'blue', 'blue']
# colors2 = ['green', 'blue', '#FFB3B3', 'black', 'turquoise', '#B22222', 'orange']
colors2 = ['green', 'blue', 'orange', 'black', 'turquoise', '#B22222', 'orange']
colors3 = ['#B22222', 'blue', '0.0', '0.4', '0.8', 'turquoise', '0.0']
markers = ['', '', '', '', '', '', '', '']
linestyles = ['solid', 'solid', 'solid', 'solid', 'solid', 'solid', 'solid']
# linestyles = ['none', 'none', 'none', 'none', 'none', 'none', 'none']
# hatchs = ['///', '\\\\\\', '++', '--', '||', '..', 'oo']
hatchs = ['/////', '\\\\\\\\\\', '|||||', '-----', '+++++', '..', 'oo']

def set_fig_legend(fig, axs, right_ax=None, ncol=None, font_size=font_size, pos="upper center", rotate=None, labelspacing=None, handletextpad=None, other_axs=None):
    handles, labels = axs[0].get_legend_handles_labels()  # Get the handles and labels of the first subplot's legend
    if right_ax != None:
        right_handles, right_labels = right_ax.get_legend_handles_labels()
        handles = [*handles, *right_handles]
        labels = [*labels, *right_labels]
    if other_axs != None:
        handles = [*handles,]
        labels = [*labels,]
        for ax in other_axs:
            h, l = ax.get_legend_handles_labels()  
            handles.append(*h)
            labels.append(*l)
        # this is a hacky fix
        hs = [handles[0], handles[3], handles[1], handles[4], handles[2], handles[5]]
        ls = [labels[0], labels[3], labels[1], labels[4], labels[2], labels[5]]
        handles = hs
        labels = ls
        # print(handles, labels)
    if ncol == None:
        ncol = len(labels)
    legend = None
    if handletextpad != None:
        legend = fig.legend(handles, labels, loc=pos, ncol=ncol, handletextpad=handletextpad, fontsize=font_size)  # Create a big legend for all subplots using the handles and labels of the first subplot's legend        
    elif font_size != None:
        legend = fig.legend(handles, labels, loc=pos, ncol=ncol, fontsize=font_size)  # Create a big legend for all subplots using the handles and labels of the first subplot's legend        
    elif labelspacing != None:
        # print(labelspacing)
        legend = fig.legend(handles, labels, loc=pos, ncol=ncol, labelspacing=labelspacing)  # Create a big legend for all subplots using the handles and labels of the first subplot's legend        
    else:
        legend = fig.legend(handles, labels, loc=pos, ncol=ncol)  # Create a big legend for all subplots using the handles and labels of the first subplot's legend

    if rotate != None:
        for text in legend.get_texts():
            text.set_rotation(rotate)

def set_ax_legend(ax, ncol=None, font_size=font_size, pos="upper center", rotate=None, labelspacing=None, handletextpad=None, right_ax=None):
    handles, labels = ax.get_legend_handles_labels()  # Get the handles and labels of the first subplot's legend
    if right_ax != None:
        right_handles, right_labels = right_ax.get_legend_handles_labels()
        handles = [*handles, *right_handles]
        labels = [*labels, *right_labels]
    if ncol == None:
        ncol = len(labels)
    legend = None
    if handletextpad != None:
        legend = ax.legend(handles, labels, loc=pos, ncol=ncol, handletextpad=handletextpad, fontsize=font_size)  # Create a big legend for all subplots using the handles and labels of the first subplot's legend        
    elif font_size != None:
        legend = ax.legend(handles, labels, loc=pos, ncol=ncol, fontsize=font_size)  # Create a big legend for all subplots using the handles and labels of the first subplot's legend        
    elif labelspacing != None:
        # print(labelspacing)
        legend = ax.legend(handles, labels, loc=pos, ncol=ncol, labelspacing=labelspacing, fontsize=font_size)  # Create a big legend for all subplots using the handles and labels of the first subplot's legend        
    else:
        legend = ax.legend(handles, labels, loc=pos, ncol=ncol)  # Create a big legend for all subplots using the handles and labels of the first subplot's legend

    if rotate != None:
        for text in legend.get_texts():
            text.set_rotation(rotate)

def set_font_size(size):
    mpl.rcParams['font.size'] = size

def plot_lines(ax, Xs, Ys, xtitle, ytitle, legends, log_scale=False, xlog_scale=False, _ymin=0, _ymax=0, nbins=7, _yticks=[], xpos = None, xtick_labels=None, xtick_rotate=None,
        colors=colors2, markers=markers, linestyles=linestyles, show_legend=False, linewidth=2, markersize=6, markeredgewidth=2, to_bottom=False, show_ylabels=True, show_xlabels=True,
        legend_loc='upper left', bbox_to_anchor=None, ygrid=False, subplot_ax=None, sci_notation=False, subplot_title=None, adjust_top=None,
        xlabel_pos=None, adjust_wspace=None, adjust_bottom=None, adjust_left=None, adjust_right=None, show_yticks=True, _xticks=None, alpha=1, _ybase=None,
        xlabel_rotate=None, ylabel_rotate=None, broken_y_limits=None, ytick_rotate=None, ytick_pad=None, ylabel_coords=None, xlabelpad=None, subplot_title_font_size=font_size, subplot_title_pad=None, subplot_title_x=None):
    assert(ax)
    num_lines = len(Xs)
    # print(num_lines)
    for X, Y, legend, c, m, l in zip(Xs, Ys, legends, colors[:num_lines], markers[:num_lines], linestyles[:num_lines]):
        ax.plot(X, Y, label=legend, color=c, marker=m, linestyle=l, linewidth=linewidth, markersize=markersize if (m != '+' and m != 'x') else markersize * 1.5, markeredgewidth=markeredgewidth, zorder=-1, alpha=alpha)
        # print(X, Y, legend)
    ax.set_xticks(Xs[0])
    if xtick_labels:
        ax.set_xticklabels(xtick_labels, rotation=xtick_rotate)
    if show_xlabels:
        ax.set_xlabel(xtitle, labelpad=xlabelpad)
        if xlabel_pos != None:
            xlabel = ax.xaxis.label  # Get the xlabel object of the bottom right subplot
            xlabel.set_position(xlabel_pos)
    if ygrid:
        if log_scale:
            ax.yaxis.set_minor_locator(mpl.ticker.AutoMinorLocator())
            ax.grid(which='both', axis='y')
    if show_ylabels:
        ax.set_ylabel(ytitle)
    if _yticks != None:
        ax.set_yticks(_yticks)
        if ytick_rotate:
            ax.tick_params(axis='y', rotation=ytick_rotate)
        if ytick_pad:
            ax.tick_params(axis='y', pad=ytick_pad)
        if ylabel_coords:
            ax.yaxis.set_label_coords(ylabel_coords[0], ylabel_coords[1])
    else:
        ax.yaxis.set_major_locator(plt.MaxNLocator(nbins=nbins))
        # plt.locator_params(axis='y', nbins=nbins)
    if not show_yticks:
        # ax.set_yticklabels([])
        ax.tick_params(axis='y', which='both', length=0, labelsize=0)
    if sci_notation:
        ax.yaxis.set_major_formatter(mpl.ticker.ScalarFormatter(useMathText=True))
        ax.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
    if xlog_scale:
        ax.set_xscale("log")
    if log_scale:
        ax.set_yscale("log")
    if _ybase != None:
        ax.yaxis.set_major_locator(mpl.ticker.LogLocator(base=_ybase))
        ax.yaxis.set_major_formatter(ScalarFormatter())
        ax.minorticks_off()
    if _xticks != None:
        ax.set_xticks(_xticks)
        ax.set_xticklabels(_xticks)
        # ax.set_yticks(_yticks)
        # ax.set_yticklabels(_yticklabels)
    if broken_y_limits != None:
        ylim1, ylim2 = broken_y_limits
    elif _ymax != 0:
        ax.set_ylim(_ymin, _ymax)
    if xpos != None:
        ax.set_xlim(xpos[0], xpos[1])
    if show_legend:
        ax.legend(loc=legend_loc, bbox_to_anchor=bbox_to_anchor)
    if subplot_title != None:
        if subplot_title_pad != None:
            assert(subplot_title_x != None)
            ax.set_title(subplot_title, fontdict={'fontsize': subplot_title_font_size}, pad=subplot_title_pad, x=subplot_title_x)
        else:
            ax.set_title(subplot_title, fontdict={'fontsize': subplot_title_font_size})
    if adjust_top != None:
        plt.subplots_adjust(top=adjust_top)
    if adjust_bottom != None:
        plt.subplots_adjust(bottom=adjust_bottom)
    if adjust_left != None:
        plt.subplots_adjust(left=adjust_left)
    if adjust_right != None:
        plt.subplots_adjust(right=adjust_right)
    if adjust_wspace != None:
        plt.subplots_adjust(wspace=adjust_wspace)
    return ax

def plot_bars(ax, Xs, Ys, xtitle, ytitle, legends, xtick_labels=None, xtick_rotate=None, log_scale=False, _ymin=0, _ymax=0, nbins=7, _yticks=[],
        colors=colors2, markers=markers, linestyles=linestyles, hatchs=hatchs, width=0.5, show_legend=False, show_ylabels=True, show_xlabels=True,
        legend_loc='upper left', bbox_to_anchor=None, ygrid=False, sci_notation=False, subplot_title=None, adjust_top=None, to_bottom=True,
        xlabel_pos=None, adjust_wspace=None, adjust_bottom=None, adjust_left=None, adjust_right=None, show_yticks=True, show_xticks=True, xlog_scale=False, ytick_rotate=None, ytick_pad=None,
        ylabel_coords=None, left_ax=None):
    num_bars = len(Xs)
    if num_bars == 1:
        shifts = [0, ]
    elif num_bars == 2:
        shifts = [-0.5 * width, 0.5 * width]
    elif num_bars == 3:
        shifts = [-1 * width, 0, 1 * width]
    elif num_bars == 4:
        shifts = [-1.5 * width, -0.5 * width, 0.5 * width, 1.5 * width]   
    elif num_bars == 5:
        shifts = [-2 * width, -1 * width, 0, 1 * width, 2 * width]   
    for i, (X, Y, legend, c, m, l, h) in enumerate(zip(Xs, Ys, legends, colors[:num_bars], markers[:num_bars], linestyles[:num_bars], hatchs[:num_bars])):
        X = [_ + shifts[i] for _ in X]
        ax.bar(X, Y, width=width, label=legend, color='white', edgecolor=c, hatch=h)
        # print(X, Y, legend)
    ax.set_xticks(Xs[0])
    if xtick_labels:
        ax.set_xticklabels(xtick_labels, rotation=xtick_rotate)
    if not show_xticks:
        ax.tick_params(axis='x', which='both', length=0, labelsize=0)
    if show_xlabels:
        ax.set_xlabel(xtitle)
        if xlabel_pos != None:
            xlabel = ax.xaxis.label  # Get the xlabel object of the bottom right subplot
            xlabel.set_position(xlabel_pos)
    if ygrid:
        # if log_scale:
        # ax.grid(which='both', axis='y', zorder=0)
        # ax.yaxis.set_minor_locator(mpl.ticker.AutoMinorLocator())
        for y in _yticks:
            ax.axhline(y=y, color='gray', linestyle='--', linewidth=0.7, zorder=0)  # Grid line in the background
        # for y in high_yticks:
            # ax_high.axhline(y=y, color='gray', linestyle='--', linewidth=0.7, zorder=0)  # Grid line in the background
    if show_ylabels:
        ax.set_ylabel(ytitle)
    if _yticks != None:
        ax.set_yticks(_yticks)
        # print(_yticks)
    else:
        # plt.locator_params(axis='y', nbins=nbins)
        ax.yaxis.set_major_locator(plt.MaxNLocator(nbins=nbins))
    if not show_yticks:
        # ax.set_yticklabels([])
        ax.tick_params(axis='y', which='both', length=0, labelsize=0)
    if ytick_rotate:
        ax.tick_params(axis='y', rotation=ytick_rotate)
    if ytick_pad:
        ax.tick_params(axis='y', pad=ytick_pad)
    if ylabel_coords:
        ax.yaxis.set_label_coords(ylabel_coords[0], ylabel_coords[1])
    if sci_notation:
        ax.yaxis.set_major_formatter(mpl.ticker.ScalarFormatter(useMathText=True))
        ax.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
    if xlog_scale:
        ax.set_xscale("log")
    if log_scale:
        ax.set_yscale("log")
    if _ymax != 0:
        ax.set_ylim(_ymin, _ymax)
    if to_bottom:
        left_ax.set_zorder(2)
        left_ax.set_frame_on(False)
        ax.set_zorder(1)
    if show_legend:
        ax.legend(loc=legend_loc, bbox_to_anchor=bbox_to_anchor)
    if subplot_title != None:
        ax.set_title(subplot_title, fontdict={'fontsize': font_size})
    if adjust_top != None:
        plt.subplots_adjust(top=adjust_top)
    if adjust_left != None:
        plt.subplots_adjust(left=adjust_left)
    if adjust_right != None:
        plt.subplots_adjust(right=adjust_right)
    if adjust_bottom != None:
        plt.subplots_adjust(bottom=adjust_bottom)
    if adjust_wspace != None:
        plt.subplots_adjust(wspace=adjust_wspace)
    return ax

def plot_broken_bars(ax, ax_high, Xs, Ys, xtitle, ytitle, legends, use_ax_highs, xtick_labels=None, xtick_rotate=None, log_scale=False, low_ymin=0, low_ymax=0, high_ymin=0, high_ymax=0, nbins=7, _yticks=[], high_yticks=[],
        colors=colors2, markers=markers, linestyles=linestyles, hatchs=hatchs, width=0.5, show_legend=False, show_ylabels=True, show_xlabels=True,
        legend_loc='upper left', bbox_to_anchor=None, ygrid=False, sci_notation=False, subplot_title=None, adjust_top=None, to_bottom=True,
        xlabel_pos=None, adjust_wspace=None, adjust_bottom=None, adjust_left=None, adjust_right=None, show_yticks=True, show_xticks=True, xlog_scale=False, ytick_rotate=None, ytick_pad=None,
        ylabel_coords=None, ylabel_pos=None, left_ax=None, ytick_font_size=None):
    num_bars = len(Xs)
    if num_bars == 1:
        shifts = [0, ]
    elif num_bars == 2:
        shifts = [-0.5 * width, 0.5 * width]
    elif num_bars == 3:
        shifts = [-1 * width, 0, 1 * width]
    elif num_bars == 4:
        shifts = [-1.5 * width, -0.5 * width, 0.5 * width, 1.5 * width]   
    elif num_bars == 5:
        shifts = [-2 * width, -1 * width, 0, 1 * width, 2 * width]   
    for i, (X, Y, legend, c, m, l, h, uh) in enumerate(zip(Xs, Ys, legends, colors[:num_bars], markers[:num_bars], linestyles[:num_bars], hatchs[:num_bars], use_ax_highs)):
        X = [_ + shifts[i] for _ in X]
        # (ax if not uh else ax_high).bar(X, Y, width=width, label=legend, color='white', edgecolor=c, hatch=h)
        ax.bar(X, Y, width=width, label=legend, color='white', edgecolor=c, hatch=h, zorder=3)
        if uh:
            ax_high.bar(X, Y, width=width, label=legend, color='white', edgecolor=c, hatch=h, zorder=3)            
        # print(X, Y, legend)
        
    # ax.spines['top'].set_visible(False)
    # ax_high.spines['bottom'].set_visible(False)
    
    d = .015  # size of the diagonal line
    kwargs = dict(transform=ax_high.transAxes, color='k', clip_on=False)
    ax_high.plot((-d, +d), (-d, +d), **kwargs)
    ax_high.plot((1 - d, 1 + d), (-d, +d), **kwargs)

    kwargs.update(transform=ax.transAxes)  # switch to the bottom subplot
    ax.plot((-d, +d), (1 - d, 1 + d), **kwargs)
    ax.plot((1 - d, 1 + d), (1 - d, 1 + d), **kwargs)
    
    if show_xticks:
        ax.set_xticks(Xs[0])
        if xtick_labels:
            ax.set_xticklabels(xtick_labels, rotation=xtick_rotate)
    else:
        ax.tick_params(axis='x', which='both', length=0, labelsize=0)
    ax_high.tick_params(axis='x', which='both', length=0, labelsize=0)
    if show_xlabels:
        ax.set_xlabel(xtitle)
        if xlabel_pos != None:
            xlabel = ax.xaxis.label  # Get the xlabel object of the bottom right subplot
            xlabel.set_position(xlabel_pos)
    if ygrid:
        # if log_scale:
        # ax.grid(which='both', axis='y', zorder=0)
        # ax.yaxis.set_minor_locator(mpl.ticker.AutoMinorLocator())
        for y in _yticks:
            ax.axhline(y=y, color='gray', linestyle='--', linewidth=0.7, zorder=0)  # Grid line in the background
        for y in high_yticks:
            ax_high.axhline(y=y, color='gray', linestyle='--', linewidth=0.7, zorder=0)  # Grid line in the background
    if show_ylabels:
        ax.set_ylabel(ytitle)
        if ylabel_pos != None:
            ylabel = ax.yaxis.label  # Get the xlabel object of the bottom right subplot
            ylabel.set_position(ylabel_pos)
    if _yticks != None:
        ax.set_yticks(_yticks)
        # print(_yticks)
        if ytick_font_size != None:
            ax.set_yticklabels([f"{ytick:.1f}" for ytick in _yticks], fontdict={'fontsize': ytick_font_size})
            ax_high.set_yticklabels([f"{ytick:.1f}" for ytick in high_yticks], fontdict={'fontsize': ytick_font_size})
    if high_yticks != None:
        ax_high.set_yticks(high_yticks)
    else:
        # plt.locator_params(axis='y', nbins=nbins)
        ax.yaxis.set_major_locator(plt.MaxNLocator(nbins=nbins))
    if not show_yticks:
        # ax.set_yticklabels([])
        ax.tick_params(axis='y', which='both', length=0, labelsize=0)
        ax_high.tick_params(axis='y', which='both', length=0, labelsize=0)
    if ytick_rotate:
        ax.tick_params(axis='y', rotation=ytick_rotate)
    if ytick_pad:
        ax.tick_params(axis='y', pad=ytick_pad)
    if ylabel_coords:
        ax.yaxis.set_label_coords(ylabel_coords[0], ylabel_coords[1])
    if sci_notation:
        ax.yaxis.set_major_formatter(mpl.ticker.ScalarFormatter(useMathText=True))
        ax.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
    if xlog_scale:
        ax.set_xscale("log")
    if log_scale:
        ax.set_yscale("log")
    if low_ymax != 0:
        ax.set_ylim(low_ymin, low_ymax)
    if high_ymax != 0 and ax_high != None:
        ax_high.set_ylim(high_ymin, high_ymax)
    if to_bottom:
        left_ax.set_zorder(2)
        left_ax.set_frame_on(False)
        ax.set_zorder(1)
    if show_legend:
        ax.legend(loc=legend_loc, bbox_to_anchor=bbox_to_anchor)
    if subplot_title != None:
        ax_high.set_title(subplot_title, fontdict={'fontsize': font_size})
    if adjust_top != None:
        plt.subplots_adjust(top=adjust_top)
    if adjust_left != None:
        plt.subplots_adjust(left=adjust_left)
    if adjust_right != None:
        plt.subplots_adjust(right=adjust_right)
    if adjust_bottom != None:
        plt.subplots_adjust(bottom=adjust_bottom)
    if adjust_wspace != None:
        plt.subplots_adjust(wspace=adjust_wspace)
    return ax

def plot_stacked_bars(ax, Xs, Ys, xtitle, ytitle, legends, xtick_labels=None, xtick_rotate=None, log_scale=False, _ymin=0, _ymax=0, nbins=7, _yticks=[], xpos = None,
        colors=colors2, hatchs=hatchs, width=0.5, show_legend=False, show_ylabels=True, show_xlabels=True,
        legend_loc='upper left', bbox_to_anchor=None, ygrid=False, sci_notation=False, subplot_title=None, adjust_top=None,
        xlabel_pos=None, adjust_wspace=None, adjust_bottom=None, adjust_left=None, adjust_right=None, show_yticks=True, xlog_scale=False, ytick_rotate=None, ytick_pad=None,
        ylabel_coords=None):
    num_bars = len(Xs)
    # if num_bars == 1:
    #     shifts = [0, ]
    # elif num_bars == 2:
    #     shifts = [-0.5 * width, 0.5 * width]
    # elif num_bars == 3:
    #     shifts = [-1 * width, 0, 1 * width]
    # elif num_bars == 4:
    #     shifts = [-1.5 * width, -0.5 * width, 0.5 * width, 1.5 * width]   
    # elif num_bars == 5:
    #     shifts = [-2 * width, -1 * width, 0, 1 * width, 2 * width]
    
    accum = np.zeros(len(Xs[0]))
    for i, (X, Y, legend, c, h) in enumerate(zip(Xs, Ys, legends, colors[:num_bars], hatchs[:num_bars])):
        # X = [_ + shifts[i] for _ in X]
        # print(X, Y, legend, c, h, accum)
        ax.bar(X, Y, width=width, bottom=accum, label=legend, color='white', edgecolor=c, hatch=h)
        accum += np.array(Y)
        
    if xtick_labels:
        ax.set_xticks(Xs[0], labels=xtick_labels, rotation=xtick_rotate)
    else:
        ax.set_xticks(Xs[0])
    if show_xlabels:
        ax.set_xlabel(xtitle)
        if xlabel_pos != None:
            xlabel = ax.xaxis.label  # Get the xlabel object of the bottom right subplot
            xlabel.set_position(xlabel_pos)
    if ygrid:
        if log_scale:
            ax.grid(which='both', axis='y')
            ax.yaxis.set_minor_locator(mpl.ticker.AutoMinorLocator())
    if show_ylabels:
        ax.set_ylabel(ytitle)
    if _yticks != None:
        ax.set_yticks(_yticks)
        # print(_yticks)
    else:
        # plt.locator_params(axis='y', nbins=nbins)
        ax.yaxis.set_major_locator(plt.MaxNLocator(nbins=nbins))
    if not show_yticks:
        # ax.set_yticklabels([])
        ax.tick_params(axis='y', which='both', length=0, labelsize=0)
    if ytick_rotate:
        ax.tick_params(axis='y', rotation=ytick_rotate)
    if ytick_pad:
        ax.tick_params(axis='y', pad=ytick_pad)
    if ylabel_coords:
        ax.yaxis.set_label_coords(ylabel_coords[0], ylabel_coords[1])
    if sci_notation:
        ax.yaxis.set_major_formatter(mpl.ticker.ScalarFormatter(useMathText=True))
        ax.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
        # ax.yaxis.set_major_formatter(mticker.ScalarFormatter())
        # ax.yaxis.get_offset_text().set_visible(False)  # Removes the default scientific notation offset
        # ax.yaxis.set_major_formatter(mticker.FuncFormatter(lambda x, _: f'{x/1000:.1f}x10^3'))
    if xlog_scale:
        ax.set_xscale("log")
    if log_scale:
        ax.set_yscale("log")
    if xpos != None:
        ax.set_xlim(xpos[0], xpos[1])
    if _ymax != 0:
        ax.set_ylim(_ymin, _ymax)
    if show_legend:
        ax.legend(loc=legend_loc, bbox_to_anchor=bbox_to_anchor)
    if subplot_title != None:
        ax.set_title(subplot_title, fontdict={'fontsize': font_size})
    if adjust_top != None:
        plt.subplots_adjust(top=adjust_top)
    if adjust_left != None:
        plt.subplots_adjust(left=adjust_left)
    if adjust_right != None:
        plt.subplots_adjust(right=adjust_right)
    if adjust_bottom != None:
        plt.subplots_adjust(bottom=adjust_bottom)
    if adjust_wspace != None:
        plt.subplots_adjust(wspace=adjust_wspace)
    return ax

def set_y_axis(ax, show_ylabels=True, ytitle="Please set ytitle", _yticks=None, show_yticks=True, nbins=7,
            sci_notation=False, log_scale=False, _ymax=0, _ymin=0):
    assert ax != None
    if show_ylabels:
        ax.set_ylabel(ytitle)
    if _yticks != None:
        ax.set_yticks(_yticks)
        # print(_yticks)
    else:
        # plt.locator_params(axis='y', nbins=nbins)
        ax.yaxis.set_major_locator(plt.MaxNLocator(nbins=nbins))
    if not show_yticks:
        # ax.set_yticklabels([])
        ax.tick_params(axis='y', which='both', length=0, labelsize=0)
    if sci_notation:
        ax.yaxis.set_major_formatter(mpl.ticker.ScalarFormatter(useMathText=True))
        ax.ticklabel_format(style='sci', axis='y', scilimits=(0,0))
    if log_scale:
        ax.set_yscale("log")
        # ax.yaxis.set_major_formatter(ScalarFormatter())
        # ax.minorticks_off()
    # if separate_xaxis:
    #     ax3 = ax.twiny()
    #     # ax3.set_xlim(Xs[0][0], Xs[0][-1])
    #     ax3.xaxis.set_visible(False)
    if _ymax != 0:
        ax.set_ylim(_ymin, _ymax)

def open_subplots(x, y, size, height_ratios=None):
    if height_ratios != None:
        fig, axs = plt.subplots(x, y, figsize=size, gridspec_kw={'height_ratios': height_ratios})
    else:
        fig, axs = plt.subplots(x, y, figsize=size)
    return fig, axs

def close_subplots(figname):
    plt.savefig(figname)
    plt.close()