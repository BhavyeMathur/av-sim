from dataclasses import dataclass

import pandas as pd

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
    plt.show()


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


__all__ = ["PlotSeries", "plot_lines", "style_plot"]
