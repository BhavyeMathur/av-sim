from __future__ import annotations

from collections import defaultdict
from typing import Sequence, Callable

import numpy as np
import pandas as pd

import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
from matplotlib.ticker import FuncFormatter

from .layers import *
from .configs import *
from .style import *


class Plot:
    def __init__(self, title: str, subtitle: str | None = None, *,
                 xlabel: str | None = None,
                 ylabel: str | None = None,
                 figsize: tuple[float, float] = (10, 6),
                 facecolor: str = "white",
                 dpi: int = 140,
                 legend: LegendConfig | None = None,
                 histogram: HistogramConfig | None = None):
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
        self._vline_layers: list[VLineLayer] = []
        self._scatter_layers: list[ScatterLayer] = []
        self._hist_layers: list[HistLayer] = []
        self._stack_layers: list[StackLayer] = []
        self._pie_layers: list[PieLayer] = []
        self._bar_layers: list[BarLayer] = []

        self._x_tick_unit: str | None = None
        self._x_tick_formatter: Callable | None = None

        self._y_tick_units: dict[int, str] = {}
        self._y_tick_formatters: dict[int, Callable] = {}

    def add_line(self, x, y, *,
                 label: str | None = None,
                 color: str | None = None,
                 linewidth: float = 2.0,
                 linestyle: str = "-",
                 alpha: float = 1.0,
                 marker: str | None = None,
                 markersize: float = 5.0,
                 axis: int = 0,

                 lower=None,
                 upper=None,

                 lower_color: str | None = None,
                 upper_color: str | None = None,
                 aux_linewidth: float = 1.25,
                 aux_linestyle: str = "--",
                 aux_alpha: float = 0.9,

                 fill_between: bool = False,
                 band_color: str | None = None,
                 band_alpha: float = 0.15) -> Plot:

        lower_style = None
        upper_style = None
        band_style = None

        if lower is not None:
            lower_style = AuxLineStyle(
                color=lower_color if lower_color is not None else color,
                linewidth=aux_linewidth,
                linestyle=aux_linestyle,
                alpha=aux_alpha,
            )

        if upper is not None:
            upper_style = AuxLineStyle(
                color=upper_color if upper_color is not None else color,
                linewidth=aux_linewidth,
                linestyle=aux_linestyle,
                alpha=aux_alpha,
            )

        if fill_between and lower is not None and upper is not None:
            band_style = BandStyle(
                color=band_color if band_color is not None else color,
                alpha=band_alpha,
            )

        self._line_layers.append(
            LineLayer(
                x=x,
                y=y,
                label=label,
                color=color,
                linewidth=linewidth,
                linestyle=linestyle,
                alpha=alpha,
                marker=marker,
                markersize=markersize,
                axis=axis,

                lower=lower,
                upper=upper,
                lower_style=lower_style,
                upper_style=upper_style,

                fill_between=(fill_between and lower is not None and upper is not None),
                band_style=band_style,
            )
        )

        return self

    def add_vline(self, x, *,
                  label: str | None = None,
                  color: str | None = None,
                  linewidth: float = 1.5,
                  linestyle: str = "--",
                  alpha: float = 0.9,
                  axis: int = 0) -> Plot:
        self._vline_layers.append(
            VLineLayer(
                x=x,
                label=label,
                color=color,
                linewidth=linewidth,
                linestyle=linestyle,
                alpha=alpha,
                axis=axis,
            )
        )
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
                 bins: int | Sequence[float] | None = None,
                 alpha: float = 1.0,
                 density: bool = False,
                 histtype: str = "stepfilled",
                 linewidth: float = 1.5) -> Plot:
        self._hist_layers.append(
            HistLayer(x=x, label=label, color=color, bins=bins, alpha=alpha, density=density, histtype=histtype,
                      linewidth=linewidth, ))
        return self

    def add_stackplot(self, x, ys, *,
                      labels: Sequence[str] | None = None,
                      colors: Sequence[str] | None = None,
                      alpha: float = 1.0,
                      baseline: str = "zero") -> Plot:
        self._stack_layers.append(
            StackLayer(x=x, ys=ys, labels=labels, colors=colors, alpha=alpha, baseline=baseline, ))
        return self

    def add_pie(self, values: Sequence[float], *,
                labels: Sequence[str] | None = None,
                colors: Sequence[str] | None = None,
                autopct: str | None = "%1.1f%%",
                startangle: float = 90,
                counterclock: bool = False,
                wedgeprops: dict | None = None) -> Plot:
        self._pie_layers.append(
            PieLayer(values=values, labels=labels, colors=colors, autopct=autopct, startangle=startangle,
                     counterclock=counterclock, wedgeprops=wedgeprops or {"linewidth": 1.0, "edgecolor": "white"}, ))
        return self

    def add_barplot(self, x, y, *,
                    label: str | None = None,
                    color: str | None = None,
                    width: float = 0.8,
                    alpha: float = 1.0,
                    axis: int = 0) -> Plot:
        self._bar_layers.append(
            BarLayer(
                x=x,
                y=y,
                label=label,
                color=color,
                width=width,
                alpha=alpha,
                axis=axis,
            )
        )
        return self

    def _has_cartesian(self) -> bool:
        return bool(
            self._line_layers
            or self._scatter_layers
            or self._hist_layers
            or self._stack_layers
            or self._bar_layers
        )

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

    def _get_or_create_axes(self, ax):
        max_axis = 0
        for layer in self._bar_layers:
            max_axis = max(max_axis, layer.axis)
        for layer in self._line_layers:
            max_axis = max(max_axis, getattr(layer, "axis", 0))
        for layer in self._scatter_layers:
            max_axis = max(max_axis, getattr(layer, "axis", 0))

        axes = [ax]

        for i in range(1, max_axis + 1):
            twin = ax.twinx()

            # never let the twin axis draw its own background/frame box
            twin.patch.set_visible(False)
            twin.spines["left"].set_visible(False)
            twin.spines["top"].set_visible(False)
            twin.spines["bottom"].set_visible(False)

            if i > 1:
                twin.spines["right"].set_position(("axes", 1 + 0.08 * (i - 1)))

            axes.append(twin)

        return axes

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

        for layer in self._vline_layers:
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

    def _legend_draw(self, ax, handles, ncols: int, standard: bool):
        if standard:
            ax.legend(handles=handles, frameon=self.legend.frameon, loc=self.legend.standard_loc)
            return

        kwargs = dict(
            handles=handles,
            frameon=self.legend.frameon,
            ncol=ncols,
            handlelength=self.legend.handlelength,
            handletextpad=self.legend.handletextpad,
            columnspacing=self.legend.columnspacing,
            borderaxespad=self.legend.borderaxespad,
        )

        if self.legend.placement == "header_right":
            ax.legend(
                loc=self.legend.fancy_loc,
                bbox_to_anchor=self.legend.fancy_bbox_to_anchor,
                **kwargs,
            )
        else:
            ax.legend(loc="best", **kwargs)

    def _apply_legend_from_axes(self, axes: list):
        if not self.legend.enabled:
            return

        if self.legend.style == "standard":
            handles = []
            labels = []
            for ax in axes:
                h, l = ax.get_legend_handles_labels()
                handles.extend(h)
                labels.extend(l)

            if labels:
                self._legend_draw(
                    axes[0],
                    handles=handles,
                    ncols=1,
                    standard=True,
                )
            return

        handles = self._collect_fancy_legend_handles()
        if not handles:
            raw_handles = []
            raw_labels = []
            for ax in axes:
                h, l = ax.get_legend_handles_labels()
                raw_handles.extend(h)
                raw_labels.extend(l)
            if not raw_labels:
                return
            handles = raw_handles

        ncols = self.legend.ncols_for(len(handles))
        self._legend_draw(axes[0], handles=handles, ncols=ncols, standard=False)

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
        axes = self._get_or_create_axes(ax)

        resolved_hist_bins = self._resolve_hist_bins_for_each_layer()

        for layer, bins in zip(self._hist_layers, resolved_hist_bins):
            x = self._clean_numeric(layer.x)
            axes[0].hist(
                x,
                bins=bins,
                label=layer.label,
                color=layer.color,
                alpha=layer.alpha,
                density=layer.density,
                histtype=layer.histtype,
                linewidth=layer.linewidth,
            )

        for layer in self._stack_layers:
            ys = np.asarray(layer.ys)
            if ys.ndim != 2:
                raise ValueError("Stackplot ys must be 2D with one series per row.")
            axes[0].stackplot(
                layer.x,
                *list(ys),
                labels=layer.labels,
                colors=layer.colors,
                alpha=layer.alpha,
                baseline=layer.baseline,
            )

        self._render_bars(axes)

        for layer in self._line_layers:
            target_ax = axes[getattr(layer, "axis", 0)]

            x = np.asarray(layer.x)
            y = np.asarray(layer.y)

            if layer.lower is not None:
                lower = np.asarray(layer.lower)
                if lower.shape != y.shape:
                    raise ValueError("Line layer lower must have the same shape as y.")
            else:
                lower = None

            if layer.upper is not None:
                upper = np.asarray(layer.upper)
                if upper.shape != y.shape:
                    raise ValueError("Line layer upper must have the same shape as y.")
            else:
                upper = None

            if layer.fill_between:
                if lower is None or upper is None:
                    raise ValueError("fill_between=True requires both lower and upper.")
                band_color = layer.band_style.color if layer.band_style else layer.color
                band_alpha = layer.band_style.alpha if layer.band_style else 0.15
                target_ax.fill_between(
                    x,
                    lower,
                    upper,
                    color=band_color,
                    alpha=band_alpha,
                    linewidth=0,
                    zorder=1,
                )

            if lower is not None and layer.lower_style is not None:
                target_ax.plot(
                    x,
                    lower,
                    color=layer.lower_style.color,
                    linewidth=layer.lower_style.linewidth,
                    linestyle=layer.lower_style.linestyle,
                    alpha=layer.lower_style.alpha,
                    label=None,
                    zorder=2,
                )

            if upper is not None and layer.upper_style is not None:
                target_ax.plot(
                    x,
                    upper,
                    color=layer.upper_style.color,
                    linewidth=layer.upper_style.linewidth,
                    linestyle=layer.upper_style.linestyle,
                    alpha=layer.upper_style.alpha,
                    label=None,
                    zorder=2,
                )

            target_ax.plot(
                x,
                y,
                label=layer.label,
                color=layer.color,
                linewidth=layer.linewidth,
                linestyle=layer.linestyle,
                alpha=layer.alpha,
                marker=layer.marker,
                markersize=layer.markersize,
                zorder=3,
            )

        for layer in self._scatter_layers:
            target_ax = axes[getattr(layer, "axis", 0)]
            target_ax.scatter(
                layer.x,
                layer.y,
                label=layer.label,
                color=layer.color,
                s=layer.s,
                alpha=layer.alpha,
            )

        for layer in self._vline_layers:
            target_ax = axes[getattr(layer, "axis", 0)]
            target_ax.axvline(
                x=layer.x,
                label=layer.label,
                color=layer.color,
                linewidth=layer.linewidth,
                linestyle=layer.linestyle,
                alpha=layer.alpha,
                zorder=4,
            )

        spine_color = "#7b8290"

        style_plot(axes[0], self.title, self.subtitle, spine_color=spine_color)
        for extra_ax in axes[1:]:
            extra_ax.spines["right"].set_color(spine_color)
            extra_ax.tick_params(axis="y", colors="#374151")

        if self.xlabel:
            axes[0].set_xlabel(self.xlabel, color="#374151")
        if self.ylabel:
            axes[0].set_ylabel(self.ylabel, color="#374151")

        self._apply_axis_tick_formatters(axes)
        self._apply_legend_from_axes(axes)

    def _render_pie(self, ax, layer: PieLayer):
        ax.pie(layer.values, labels=layer.labels, colors=layer.colors, autopct=layer.autopct,
               startangle=layer.startangle, counterclock=layer.counterclock, wedgeprops=layer.wedgeprops)
        ax.set_aspect("equal")
        style_pie(ax, self.title, self.subtitle)

    def _render_bars(self, axes: list):
        if not self._bar_layers:
            return

        layers_by_axis = defaultdict(list)
        for layer in self._bar_layers:
            layers_by_axis[layer.axis].append(layer)

        for axis_idx, layers in layers_by_axis.items():
            cur_ax = axes[axis_idx]
            n = len(layers)

            for j, layer in enumerate(layers):
                x = np.asarray(layer.x)
                y = np.asarray(layer.y)

                if len(x) != len(y):
                    raise ValueError("Bar layer x and y must have the same length.")

                # Handle datetime/categorical/numeric x
                if np.issubdtype(x.dtype, np.number):
                    x_pos = x.astype(float)
                    base_width = layer.width
                else:
                    # categorical / object / datetime-like -> place at integer positions
                    x_pos = np.arange(len(x), dtype=float)
                    base_width = layer.width

                bar_width = base_width / max(n, 1)
                offset = (j - (n - 1) / 2.0) * bar_width

                cur_ax.bar(x_pos + offset, y,
                           width=bar_width, label=layer.label, color=layer.color, alpha=layer.alpha, align="center")

                # Set tick labels once per axis for non-numeric x
                if not np.issubdtype(x.dtype, np.number):
                    cur_ax.set_xticks(x_pos)
                    cur_ax.set_xticklabels(x)

            # style extra y-axes a bit
            if axis_idx > 0:
                cur_ax.spines["top"].set_visible(False)
                cur_ax.spines["left"].set_visible(False)

    @staticmethod
    def _make_unit_formatter(unit: str, decimals: int | None = None, prefix: bool = False):
        def _fmt(x, pos=None):
            if np.isclose(x, round(x)):
                s = f"{int(round(x))}"
            elif decimals is not None:
                s = f"{x:.{decimals}f}"
            else:
                s = f"{x:g}"

            return f"{unit}{s}" if prefix else f"{s}{unit}"

        return _fmt

    def set_x_axis_unit(self, unit: str, *,
                        decimals: int | None = None,
                        prefix: bool = False) -> Plot:
        self._x_tick_unit = unit
        self._x_tick_formatter = self._make_unit_formatter(unit, decimals=decimals, prefix=prefix)
        return self

    def set_y_axis_unit(self, unit: str, *,
                        axis: int = 0,
                        decimals: int | None = None,
                        prefix: bool = False) -> Plot:
        self._y_tick_units[axis] = unit
        self._y_tick_formatters[axis] = self._make_unit_formatter(unit, decimals=decimals, prefix=prefix)
        return self

    def _apply_axis_tick_formatters(self, axes: list):
        if self._x_tick_formatter is not None:
            axes[0].xaxis.set_major_formatter(FuncFormatter(self._x_tick_formatter))

        for axis_idx, formatter in self._y_tick_formatters.items():
            if 0 <= axis_idx < len(axes):
                axes[axis_idx].yaxis.set_major_formatter(FuncFormatter(formatter))

    def save(self, path: str, **savefig_kwargs) -> None:
        fig, _ = self.render()
        fig.savefig(path, bbox_inches="tight", **savefig_kwargs)
        plt.close(fig)

    def show(self) -> None:
        fig, _ = self.render()
        plt.show()


__all__ = ["Plot", "LegendConfig", "HistogramConfig"]
