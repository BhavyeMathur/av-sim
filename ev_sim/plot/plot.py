from __future__ import annotations

from dataclasses import dataclass, field
from math import ceil
from typing import Literal, Sequence

import numpy as np
import pandas as pd

import matplotlib.pyplot as plt
import matplotlib.dates as mdates
from matplotlib.lines import Line2D
from matplotlib.patches import Patch


def _is_datetime_axis(ax) -> bool:
    try:
        converter = ax.xaxis.converter
        return converter is not None or (len(ax.lines) > 0 and any(
            np.issubdtype(np.asarray(line.get_xdata()).dtype, np.datetime64) for line in ax.lines))
    except Exception:
        return False


def _format_datetime_axis(ax, spine_color="#7b8290"):
    locator = mdates.AutoDateLocator()
    formatter = mdates.ConciseDateFormatter(locator)
    ax.xaxis.set_major_locator(locator)
    ax.xaxis.set_major_formatter(formatter)
    ax.tick_params(axis="x", colors=spine_color)
    ax.tick_params(axis="y", colors=spine_color)


def style_title(ax, title: str, subtitle: str | None = None, *, title_color="#111827", subtitle_color="#6b7280",
                subtitle_size: int = 9, ):
    ax.set_title(title, loc="left", fontweight="bold", color=title_color, pad=20)
    if subtitle:
        ax.text(0.0, 1.02, subtitle, transform=ax.transAxes, ha="left", va="bottom", fontsize=subtitle_size,
                color=subtitle_color, )


def style_plot(ax, title: str, subtitle: str | None = None, *, spine_color="#7b8290", grid_color="#374151",
               title_color="#111827", subtitle_color="#6b7280", ax_color="#000", ):
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


def style_pie(ax, title: str, subtitle: str | None = None, *, title_color="#111827", subtitle_color="#6b7280"):
    style_title(ax, title, subtitle=subtitle, title_color=title_color, subtitle_color=subtitle_color)


LegendStyle = Literal["standard", "fancy"]
LegendPlacement = Literal["best", "header_right"]


@dataclass
class LegendConfig:
    enabled: bool = True
    style: LegendStyle = "standard"
    placement: LegendPlacement = "header_right"
    max_rows: int = 2
    frameon: bool = False

    # Auto layout knobs
    handlelength: float = 0.75
    columnspacing: float = 1.2
    handletextpad: float = 0.45
    borderaxespad: float = 0.0

    # Marker sizing
    min_marker_size: float = 6.0
    max_marker_size: float = 8.0

    # Standard legend defaults
    standard_loc: str = "best"

    # Fancy legend defaults
    fancy_loc: str = "upper right"
    fancy_bbox_to_anchor: tuple[float, float] = (0.98, 1.15)

    def ncols_for(self, n_items: int) -> int:
        if n_items <= 0:
            return 1
        # At most max_rows rows
        return max(1, ceil(n_items / self.max_rows))

    def marker_size_for(self, n_items: int) -> float:
        if n_items <= 4:
            return self.max_marker_size
        if n_items <= 8:
            return 7.0
        return self.min_marker_size


@dataclass
class HistogramConfig:
    shared_edges: bool = True
    default_bins: int = 50
    range: tuple[float, float] | None = None


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
    bins: int | Sequence[float] | None = None
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


@dataclass
class StackLayer:
    x: Sequence
    ys: Sequence[Sequence[float]] | np.ndarray
    labels: Sequence[str] | None = None
    colors: Sequence[str] | None = None
    alpha: float = 1.0
    baseline: str = "zero"


class Plot:
    def __init__(self, title: str, subtitle: str | None = None, *, xlabel: str | None = None, ylabel: str | None = None,
                 figsize: tuple[float, float] = (10, 6), facecolor: str = "white", dpi: int = 140,
                 legend: LegendConfig | None = None, histogram: HistogramConfig | None = None):
        self.title = title
        self.subtitle = subtitle
        self.xlabel = xlabel
        self.ylabel = ylabel
        self.figsize = figsize
        self.facecolor = facecolor
        self.dpi = dpi

        self.legend = legend or LegendConfig()
        self.histogram = histogram or HistogramConfig()

        self._line_layers: list[LineLayer] = []
        self._scatter_layers: list[ScatterLayer] = []
        self._hist_layers: list[HistLayer] = []
        self._stack_layers: list[StackLayer] = []
        self._pie_layers: list[PieLayer] = []

    # ---------- add layers ----------

    def add_line(self, x, y, *, label: str | None = None, color: str | None = None, linewidth: float = 2.0,
                 linestyle: str = "-", alpha: float = 1.0, marker: str | None = None,
                 markersize: float = 5.0, ) -> Plot:
        self._line_layers.append(
            LineLayer(x=x, y=y, label=label, color=color, linewidth=linewidth, linestyle=linestyle, alpha=alpha,
                      marker=marker, markersize=markersize, ))
        return self

    def add_scatter(self, x, y, *, label: str | None = None, color: str | None = None, s: float = 24.0,
                    alpha: float = 0.9) -> Plot:
        self._scatter_layers.append(ScatterLayer(x=x, y=y, label=label, color=color, s=s, alpha=alpha))
        return self

    def add_hist(self, x, *, label: str | None = None, color: str | None = None,
                 bins: int | Sequence[float] | None = None, alpha: float = 1.0, density: bool = False,
                 histtype: str = "stepfilled", linewidth: float = 1.5) -> Plot:
        self._hist_layers.append(
            HistLayer(x=x, label=label, color=color, bins=bins, alpha=alpha, density=density, histtype=histtype,
                      linewidth=linewidth, ))
        return self

    def add_stackplot(self, x, ys, *, labels: Sequence[str] | None = None, colors: Sequence[str] | None = None,
                      alpha: float = 1.0, baseline: str = "zero", ) -> Plot:
        self._stack_layers.append(
            StackLayer(x=x, ys=ys, labels=labels, colors=colors, alpha=alpha, baseline=baseline, ))
        return self

    def add_pie(self, values: Sequence[float], *, labels: Sequence[str] | None = None,
                colors: Sequence[str] | None = None, autopct: str | None = "%1.1f%%", startangle: float = 90,
                counterclock: bool = False, wedgeprops: dict | None = None, ) -> Plot:
        self._pie_layers.append(
            PieLayer(values=values, labels=labels, colors=colors, autopct=autopct, startangle=startangle,
                     counterclock=counterclock, wedgeprops=wedgeprops or {"linewidth": 1.0, "edgecolor": "white"}, ))
        return self

    # ---------- helpers ----------

    def _has_cartesian(self) -> bool:
        return bool(self._line_layers or self._scatter_layers or self._hist_layers or self._stack_layers)

    def _has_pie(self) -> bool:
        return bool(self._pie_layers)

    @staticmethod
    def _clean_numeric(x) -> np.ndarray:
        x = np.asarray(x)
        return x[~pd.isna(x)]

    def _resolve_shared_hist_bins(self) -> int | np.ndarray | None:
        if not self._hist_layers:
            return None

        # Collect all explicitly provided bins from histogram layers.
        specified = [layer.bins for layer in self._hist_layers if layer.bins is not None]

        # Case 1: some layer explicitly supplied bin edges
        explicit_edges = [np.asarray(b, dtype=float) for b in specified if not np.isscalar(b)]
        if explicit_edges:
            first = explicit_edges[0]
            for edges in explicit_edges[1:]:
                if not np.array_equal(edges, first):
                    raise ValueError("All explicitly provided histogram edge arrays must match.")
            # If some other layer specified an integer bins count as well, that's ambiguous.
            if any(np.isscalar(b) for b in specified):
                raise ValueError("Do not mix explicit histogram edge arrays with integer bin counts.")
            return first

        # Case 2: one or more layers specified an integer bin count.
        specified_counts = {int(b) for b in specified if np.isscalar(b)}
        if len(specified_counts) > 1:
            raise ValueError("All explicitly provided histogram bin counts must match when edges are shared.")
        n_bins = next(iter(specified_counts), self.histogram.default_bins)

        all_x = [self._clean_numeric(layer.x) for layer in self._hist_layers]
        all_x = [x for x in all_x if x.size > 0]
        if not all_x:
            return None

        if self.histogram.range is not None:
            xmin, xmax = self.histogram.range
        else:
            x_all = np.concatenate(all_x)
            xmin = float(np.min(x_all))
            xmax = float(np.max(x_all))

            if xmin == xmax:
                half_width = 0.5 if xmin == 0 else 0.5 * abs(xmin)
                xmin -= half_width
                xmax += half_width

        return np.linspace(xmin, xmax, n_bins + 1)

    def _resolve_hist_bins_for_each_layer(self) -> list[int | Sequence[float] | np.ndarray | None]:
        if not self._hist_layers:
            return []

        if self.histogram.shared_edges:
            shared = self._resolve_shared_hist_bins()
            return [shared] * len(self._hist_layers)

        resolved = []
        for layer in self._hist_layers:
            if layer.bins is not None:
                resolved.append(layer.bins)
            else:
                resolved.append(self.histogram.default_bins)
        return resolved

    def _collect_fancy_legend_handles(self):
        handles = []

        # Prefer custom handles for stackplots.
        for layer in self._stack_layers:
            if not layer.labels:
                continue
            colors = list(layer.colors) if layer.colors is not None else [None] * len(layer.labels)
            for label, color in zip(layer.labels, colors):
                handles.append(("stack", label, color))

        # Histograms
        for layer in self._hist_layers:
            if layer.label:
                handles.append(("hist", layer.label, layer.color))

        # Lines and scatters
        for layer in self._line_layers:
            if layer.label:
                handles.append(("line", layer.label, layer.color))

        for layer in self._scatter_layers:
            if layer.label:
                handles.append(("scatter", layer.label, layer.color))

        if not handles:
            return []

        marker_size = self.legend.marker_size_for(len(handles))
        out = []

        for kind, label, color in handles:
            out.append(Line2D([0], [0], marker="o", linestyle="None", markersize=marker_size, markerfacecolor=color,
                              markeredgecolor=color, label=label))
        return out

    def _apply_legend(self, ax):
        if not self.legend.enabled:
            return

        if self.legend.style == "standard":
            handles, labels = ax.get_legend_handles_labels()
            if labels:
                ax.legend(frameon=self.legend.frameon, loc=self.legend.standard_loc)
            return

        handles = self._collect_fancy_legend_handles()
        if not handles:
            handles, labels = ax.get_legend_handles_labels()
            if not labels:
                return
            handles = handles or ax.get_legend_handles_labels()[0]

        ncols = self.legend.ncols_for(len(handles))

        if self.legend.placement == "header_right":
            ax.legend(handles=handles, frameon=self.legend.frameon, loc=self.legend.fancy_loc,
                      bbox_to_anchor=self.legend.fancy_bbox_to_anchor, ncol=ncols,
                      columnspacing=self.legend.columnspacing, handletextpad=self.legend.handletextpad,
                      borderaxespad=self.legend.borderaxespad, handlelength=self.legend.handlelength)
        else:
            ax.legend(handles=handles, frameon=self.legend.frameon, loc="best", ncol=ncols,
                      columnspacing=self.legend.columnspacing, handletextpad=self.legend.handletextpad,
                      borderaxespad=self.legend.borderaxespad, handlelength=self.legend.handlelength)

    # ---------- rendering ----------

    def render(self):
        if not self._has_cartesian() and not self._has_pie():
            raise ValueError("No layers added.")
        if self._has_cartesian() and self._has_pie():
            raise ValueError("Do not mix pie charts with cartesian layers in this Plot class.")

        fig, ax = plt.subplots(figsize=self.figsize, dpi=self.dpi, facecolor=self.facecolor)
        ax.set_facecolor(self.facecolor)

        if self._has_pie():
            if len(self._pie_layers) > 1:
                raise ValueError("Only one pie layer is supported per figure.")
            self._render_pie(ax, self._pie_layers[0])
            fig.tight_layout()
        else:
            self._render_cartesian(ax)
            # extra space for header legend if needed
            if self.legend.enabled and self.legend.style == "fancy" and self.legend.placement == "header_right":
                fig.tight_layout(rect=[0, 0, 1, 0.92])
            else:
                fig.tight_layout()

        return fig, ax

    def _render_cartesian(self, ax):
        resolved_hist_bins = self._resolve_hist_bins_for_each_layer()

        for layer, bins in zip(self._hist_layers, resolved_hist_bins):
            x = self._clean_numeric(layer.x)
            ax.hist(x, bins=bins, label=layer.label, color=layer.color, alpha=layer.alpha, density=layer.density,
                    histtype=layer.histtype, linewidth=layer.linewidth, )

        for layer in self._stack_layers:
            ys = np.asarray(layer.ys)
            if ys.ndim != 2:
                raise ValueError("Stackplot ys must be 2D with one series per row.")
            ax.stackplot(layer.x, *list(ys), labels=layer.labels, colors=layer.colors, alpha=layer.alpha,
                         baseline=layer.baseline, )

        for layer in self._line_layers:
            ax.plot(layer.x, layer.y, label=layer.label, color=layer.color, linewidth=layer.linewidth,
                    linestyle=layer.linestyle, alpha=layer.alpha, marker=layer.marker, markersize=layer.markersize, )

        for layer in self._scatter_layers:
            ax.scatter(layer.x, layer.y, label=layer.label, color=layer.color, s=layer.s, alpha=layer.alpha, )

        style_plot(ax, self.title, self.subtitle)

        if self.xlabel:
            ax.set_xlabel(self.xlabel, color="#374151")
        if self.ylabel:
            ax.set_ylabel(self.ylabel, color="#374151")

        self._apply_legend(ax)

    def _render_pie(self, ax, layer: PieLayer):
        ax.pie(layer.values, labels=layer.labels, colors=layer.colors, autopct=layer.autopct,
               startangle=layer.startangle, counterclock=layer.counterclock, wedgeprops=layer.wedgeprops, )
        ax.set_aspect("equal")
        style_pie(ax, self.title, self.subtitle)

    def save(self, path: str, **savefig_kwargs) -> None:
        fig, _ = self.render()
        fig.savefig(path, bbox_inches="tight", **savefig_kwargs)
        plt.close(fig)

    def show(self) -> None:
        fig, _ = self.render()
        plt.show()


__all__ = ["Plot", "LegendConfig", "HistogramConfig", "style_plot"]
