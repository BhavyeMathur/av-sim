from __future__ import annotations

from dataclasses import dataclass, field
from typing import Sequence

import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import matplotlib.dates as mdates


def _is_datetime_axis(ax) -> bool:
    try:
        converter = ax.xaxis.converter
        return converter is not None or len(ax.lines) > 0 and any(
            np.issubdtype(np.asarray(line.get_xdata()).dtype, np.datetime64)
            for line in ax.lines
        )
    except Exception:
        return False


def _format_datetime_axis(ax, spine_color="#7b8290"):
    locator = mdates.AutoDateLocator()
    formatter = mdates.ConciseDateFormatter(locator)
    ax.xaxis.set_major_locator(locator)
    ax.xaxis.set_major_formatter(formatter)
    ax.tick_params(axis="x", colors=spine_color)
    ax.tick_params(axis="y", colors=spine_color)


def style_title(ax, title: str, subtitle: str | None = None, *,
                title_color="#111827", subtitle_color="#6b7280", subtitle_size: int = 9, **_):
    ax.set_title(title, loc="left", fontweight="bold", color=title_color, pad=20)
    if subtitle:
        ax.text(0.0, 1.02, subtitle, transform=ax.transAxes, ha="left", va="bottom",
                fontsize=subtitle_size, color=subtitle_color)


def style_plot(ax, title: str, subtitle: str | None = None, *,
               spine_color="#7b8290", grid_color="#374151",
               title_color="#111827", subtitle_color="#6b7280", ax_color="#000"):
    ax.grid(axis="y", linestyle=(0, (1.2, 2.4)), linewidth=1.0, color=grid_color, alpha=0.55)

    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    ax.spines["left"].set_color(spine_color)
    ax.spines["bottom"].set_color(spine_color)

    ax.tick_params(axis="x", colors=ax_color)
    ax.tick_params(axis="y", colors=ax_color)

    if _is_datetime_axis(ax):
        _format_datetime_axis(ax, spine_color=spine_color)

    style_title(ax, title, subtitle=subtitle, title_color=title_color, subtitle_color=subtitle_color)
    plt.tight_layout()


def style_pie(ax, title: str, subtitle: str | None = None, *,
              title_color="#111827", subtitle_color="#6b7280"):
    style_title(ax, title, subtitle=subtitle, title_color=title_color, subtitle_color=subtitle_color)
    plt.tight_layout()


@dataclass
class LineLayer:
    x: Sequence
    y: Sequence
    label: str | None = None
    color: str | None = None
    linewidth: float = 2.0
    linestyle: str = "-"
    alpha: float = 1.0
    marker: str | None = None
    markersize: float = 5.0


@dataclass
class ScatterLayer:
    x: Sequence
    y: Sequence
    label: str | None = None
    color: str | None = None
    s: float = 24.0
    alpha: float = 0.9


@dataclass
class HistLayer:
    x: Sequence
    label: str | None = None
    color: str | None = None
    bins: int | Sequence[float] = 30
    alpha: float = 0.75
    density: bool = False
    histtype: str = "stepfilled"
    linewidth: float = 1.5


@dataclass
class PieLayer:
    values: Sequence[float]
    labels: Sequence[str] | None = None
    colors: Sequence[str] | None = None
    autopct: str | None = "%1.1f%%"
    startangle: float = 90
    counterclock: bool = False
    wedgeprops: dict = field(default_factory=lambda: {"linewidth": 1.0, "edgecolor": "white"})


class Plot:
    def __init__(self, title: str, subtitle: str | None = None, *,
                 xlabel: str | None = None,
                 ylabel: str | None = None,
                 figsize: tuple[float, float] = (10, 6),
                 facecolor: str = "white",
                 dpi: int = 140,
                 legend: bool = True):
        self.title = title
        self.subtitle = subtitle
        self.xlabel = xlabel
        self.ylabel = ylabel
        self.figsize = figsize
        self.facecolor = facecolor
        self.dpi = dpi
        self.legend = legend

        self._line_layers: list[LineLayer] = []
        self._scatter_layers: list[ScatterLayer] = []
        self._hist_layers: list[HistLayer] = []
        self._pie_layers: list[PieLayer] = []

    def add_line(self, x, y, *,
                 label: str | None = None,
                 color: str | None = None,
                 linewidth: float = 2.0,
                 linestyle: str = "-",
                 alpha: float = 1.0,
                 marker: str | None = None,
                 markersize: float = 5.0) -> Plot:
        self._line_layers.append(LineLayer(
            x=x, y=y, label=label, color=color,
            linewidth=linewidth, linestyle=linestyle,
            alpha=alpha, marker=marker, markersize=markersize))
        return self

    def add_scatter(self, x, y, *,
                    label: str | None = None,
                    color: str | None = None,
                    s: float = 24.0,
                    alpha: float = 0.9) -> Plot:
        self._scatter_layers.append(ScatterLayer(x=x, y=y, label=label, color=color, s=s, alpha=alpha))
        return self

    def add_hist(self, x, *,
                 label: str | None = None,
                 color: str | None = None,
                 bins: int | Sequence[float] = 50,
                 alpha: float = 1.0,
                 density: bool = False,
                 histtype: str = "stepfilled",
                 linewidth: float = 1.5) -> Plot:
        self._hist_layers.append(HistLayer(
            x=x, label=label, color=color, bins=bins,
            alpha=alpha, density=density,
            histtype=histtype, linewidth=linewidth))
        return self

    def add_pie(self, values: Sequence[float], *,
                labels: Sequence[str] | None = None,
                colors: Sequence[str] | None = None,
                autopct: str | None = "%1.1f%%",
                startangle: float = 90,
                counterclock: bool = False,
                wedgeprops: dict | None = None) -> Plot:
        self._pie_layers.append(PieLayer(
            values=values,
            labels=labels,
            colors=colors,
            autopct=autopct,
            startangle=startangle,
            counterclock=counterclock,
            wedgeprops=wedgeprops or {"linewidth": 1.0, "edgecolor": "white"}))
        return self

    def _has_cartesian(self) -> bool:
        return bool(self._line_layers or self._scatter_layers or self._hist_layers)

    def _has_pie(self) -> bool:
        return bool(self._pie_layers)

    def render(self):
        if not self._has_cartesian() and not self._has_pie():
            raise ValueError("No layers added.")

        if self._has_cartesian() and self._has_pie():
            raise ValueError("Do not mix pie charts with line/scatter/hist in this simple Plot class.")

        fig, ax = plt.subplots(figsize=self.figsize, dpi=self.dpi, facecolor=self.facecolor)
        ax.set_facecolor(self.facecolor)

        if self._has_pie():
            if len(self._pie_layers) > 1:
                raise ValueError("This simple Plot class supports one pie layer per figure.")
            self._render_pie(ax, self._pie_layers[0])
        else:
            self._render_cartesian(ax)

        fig.tight_layout()
        return fig, ax

    def _render_cartesian(self, ax):
        for layer in self._hist_layers:
            x = np.asarray(layer.x)
            x = x[~pd.isna(x)]
            ax.hist(x,
                    bins=layer.bins,
                    label=layer.label,
                    color=layer.color,
                    alpha=layer.alpha,
                    density=layer.density,
                    histtype=layer.histtype,
                    linewidth=layer.linewidth)

        for layer in self._line_layers:
            ax.plot(layer.x, layer.y,
                    label=layer.label,
                    color=layer.color,
                    linewidth=layer.linewidth,
                    linestyle=layer.linestyle,
                    alpha=layer.alpha,
                    marker=layer.marker,
                    markersize=layer.markersize)

        for layer in self._scatter_layers:
            ax.scatter(layer.x, layer.y,
                       label=layer.label,
                       color=layer.color,
                       s=layer.s,
                       alpha=layer.alpha, )

        style_plot(ax, self.title, self.subtitle)

        if self.xlabel:
            ax.set_xlabel(self.xlabel, color="#374151")
        if self.ylabel:
            ax.set_ylabel(self.ylabel, color="#374151")

        if self.legend:
            handles, labels = ax.get_legend_handles_labels()
            if labels:
                ax.legend(frameon=False, loc="best")

    def _render_pie(self, ax, layer: PieLayer):
        ax.pie(layer.values,
               labels=layer.labels,
               colors=layer.colors,
               autopct=layer.autopct,
               startangle=layer.startangle,
               counterclock=layer.counterclock,
               wedgeprops=layer.wedgeprops)
        ax.set_aspect("equal")
        style_pie(ax, self.title, self.subtitle)

    def save(self, path: str, **savefig_kwargs) -> None:
        fig, _ = self.render()
        fig.savefig(path, bbox_inches="tight", **savefig_kwargs)
        plt.close(fig)

    def show(self) -> None:
        fig, _ = self.render()
        plt.show()
