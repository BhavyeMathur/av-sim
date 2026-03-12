from dataclasses import dataclass

import pandas as pd
import numpy as np

import matplotlib.pyplot as plt
import matplotlib.dates as mdates
import matplotlib.ticker as ticker
from matplotlib.lines import Line2D

plt.rcParams.update({
    "font.family": "sans-serif",
    "font.sans-serif": ["Inter", "Roboto", "Arial"]
})


@dataclass
class PlotSeries:
    data: pd.Series
    name: str
    color: str
    style: str = "solid"
    width: int = 1.5
    legend: bool = True


@dataclass
class SeriesStyling:
    label: str
    color: str


def _format_datetime_axis(ax: plt.Axes, spine_color: str):
    xmin, xmax = ax.get_xlim()
    x0 = pd.Timestamp(mdates.num2date(xmin))
    xn = pd.Timestamp(mdates.num2date(xmax))
    n_days = (xn - x0).days

    start_midnight_next = (x0.normalize() + pd.Timedelta(days=1))
    midnights = pd.date_range(start_midnight_next, periods=10_000, freq="D")
    major_ticks_dt = [x0] + [t for t in midnights if xmin <= mdates.date2num(t) <= xmax]
    major_ticks = [mdates.date2num(t) for t in major_ticks_dt]

    ax.xaxis.set_major_locator(ticker.FixedLocator(major_ticks))
    ax.xaxis.set_major_formatter(mdates.DateFormatter("%d %b"))

    if n_days <= 2:
        hours = [3, 6, 9, 12, 15, 18, 21]
        format = "%H:%M"
    elif n_days == 3:
        hours = [6, 9, 12, 15, 18]
        format = "%H:%M"
    elif n_days <= 6:
        hours = [6, 9, 12, 15, 18]
        format = "%H"
    else:
        hours = [8, 12, 16]
        format = "%H"

    ax.xaxis.set_minor_locator(mdates.HourLocator(byhour=hours))
    ax.xaxis.set_minor_formatter(mdates.DateFormatter(format))

    ax.tick_params(axis="y", color=spine_color, length=2, labelsize=9)
    ax.tick_params(axis="x", which="minor", labelsize=8, pad=2, colors=spine_color)
    ax.tick_params(axis="x", which="major", length=0)


def _is_datetime_axis(ax):
    return isinstance(ax.xaxis.get_converter(), mdates.DateConverter)


def style_plot(ax, title: str, subtitle: str = None,
               spine_color="#7b8290", grid_color="#374151", title_color="#111827", subtitle_color="#6b7280"):
    ax.grid(axis="y", linestyle=(0, (1.2, 2.4)), linewidth=1.0, color=grid_color, alpha=0.55)

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color(spine_color)
    ax.spines["bottom"].set_color(spine_color)

    if _is_datetime_axis(ax):
        _format_datetime_axis(ax, spine_color=spine_color)

    ax.set_title(title, loc="left", fontweight="bold", color=title_color, pad=20)
    if subtitle:
        ax.text(0.0, 1.02, subtitle, transform=ax.transAxes, ha="left", va="bottom", fontsize=9, color=subtitle_color)

    plt.tight_layout()


def plot_lines(*data: PlotSeries, title: str, subtitle: str = None, figsize: tuple = (10, 3),
               spine_color="#7b8290", grid_color="#374151", title_color="#111827", subtitle_color="#6b7280"):
    fig, ax = plt.subplots(figsize=figsize)
    for i, series in enumerate(data):
        ax.plot(series.data.index, series.data,
                linewidth=series.width, color=series.color, linestyle=series.style)

    handles = [
        Line2D([0], [0], marker="o", markersize=7,
               markerfacecolor=series.color, markeredgecolor=series.color, label=series.name)
        for i, series in enumerate(data) if series.legend
    ]
    ax.legend(handles=handles, bbox_to_anchor=(0.93, 1.2), ncol=2, frameon=False, handlelength=0.1)

    style_plot(ax, title, subtitle, spine_color, grid_color, title_color, subtitle_color)


def _map_values_to_slot(v, xmin, xmax, left, right):
    # maps v in [xmin,xmax] -> v' in [left,right]
    return left + (v - xmin) * (right - left) / (xmax - xmin)


def plot_histogram_on_axis(data, ax, styles, title: str = "", bins: int = 100, subtitle: str = "",
                           legend: bool = None, minimal: bool = False, **kwargs):
    series = data.columns

    ax.hist([data[s] for s in series],
            color=[styles[s].color for s in series],
            label=[styles[s].label for s in series],
            stacked=True, bins=bins, **kwargs)

    if minimal:
        ax.set_xticks([])
        ax.set_yticks([])
        ax.spines["left"].set_visible(False)

    if legend is True or not minimal:
        ax.legend()

    style_plot(ax, title=title, subtitle=subtitle)


def plot_evolving_histograms(data, index, styles, title: str = "", k: int = 6, subtitle: str = ""):
    fig = plt.figure(figsize=(12, 6))
    gs = fig.add_gridspec(2, 1, height_ratios=[2, 1], hspace=0.2, wspace=0.1)

    ax1 = fig.add_subplot(gs[0])
    ax2 = fig.add_subplot(gs[1])
    ax2_hist = ax2.twinx()

    # ----------------
    # main histogram
    ax1.set_yticklabels([])
    plot_histogram_on_axis(data, ax1, styles, title=title, subtitle=subtitle, bins=200)

    # ----------------
    # rolling mean plot
    for s in data.columns:
        y = pd.Series(data[s].values, index=index).sort_index()
        bins = np.linspace(index.min(), index.max(), 100)

        y_rm = y.groupby(pd.cut(y.index, bins=bins)).mean()
        x = bins[:-1][:len(y_rm)]

        ax2.plot(x, y_rm, color=styles[s].color, linewidth=1.5, label=styles[s].label)

    style_plot(ax2, title="", grid_color="#fff")

    # ----------------
    # smaller panel of evolving histograms
    bins_per_slot = 50
    edges = np.linspace(index.min(), index.max(), k + 1)

    for i in range(k):
        left, right = edges[i], edges[i + 1]
        mask = (index >= left) & (index < right)

        mock = []
        for s in data.columns:
            xmin = np.quantile(data[s], 0.01) * 0.5
            xmax = np.quantile(data[s], 0.99) * 1.5

            v = data.loc[mask, s].to_numpy()
            v = v[(v > xmin) & (v < xmax)]
            mock.append(_map_values_to_slot(v, xmin, xmax, left, right))

        slot_bins = np.linspace(left, right, bins_per_slot + 1)
        ax2_hist.hist(mock, bins=slot_bins, stacked=True, color=[styles[s].color for s in data.columns], alpha=0.4)

    style_plot(ax2_hist, title="")
    ax2_hist.yaxis.set_visible(False)


__all__ = ["PlotSeries", "SeriesStyling",
           "plot_lines", "style_plot", "plot_histogram_on_axis", "plot_evolving_histograms"]
