from __future__ import annotations
from typing import Iterable, Literal
from dataclasses import dataclass

import numpy as np
import pandas as pd
import geopandas as gpd

import datashader as ds
import datashader.transfer_functions as tf
from PIL import Image
from pyproj import Transformer

from shapely.geometry import shape
from shapely.ops import transform as shapely_transform

from ev_sim.plot.blend import BLEND_FUNCS
from ev_sim.plot.cmaps import CMAP_TYPE, default_cmap
from .basemap import *
from .geolayers import *


@dataclass
class RenderContext:
    width: int
    height: int
    x_range: tuple[float, float]
    y_range: tuple[float, float]
    canvas: ds.Canvas
    grid_canvases: dict[tuple[int, int], ds.Canvas]
    base_img: Image.Image
    labels_img: Image.Image | None


def _make_grid_canvas(bins: tuple[int, int], x_range: tuple[float, float], y_range: tuple[float, float]) -> ds.Canvas:
    nx, ny = bins
    return ds.Canvas(plot_width=nx, plot_height=ny, x_range=x_range, y_range=y_range)


class GeoPlot:
    """
    Example
    -------
    plot = GeoPlot(width=1200, labels=True)
    plot.add_lines(df1)
    plot.add_lines(df2, cmap=["#000000", "#2244ff", "#88ccff"])
    img = plot.render()
    plot.save("map.png")
    """

    def __init__(self, *,
                 width: int = 1200,
                 bbox_latlon: tuple[float, float, float, float] | None = None,
                 q: float = 0.001,
                 labels: bool = False,
                 basemap_style: BasemapStyle | None = None,
                 background_rgba: tuple[int, int, int, int] | None = None,
                 blend="add") -> None:
        self.width = width
        self.bbox_latlon = bbox_latlon
        self.q = q
        self.labels = labels
        self.basemap_style = basemap_style or BasemapStyle()
        self.background_rgba = background_rgba
        self.blend = BLEND_FUNCS[blend] if isinstance(blend, str) else blend

        self._layers: list[LineLayer | GridLayer | PolygonLayer] = []
        self._transformer = Transformer.from_crs("EPSG:4326", "EPSG:3857", always_xy=True)

    def add_lines(self, df, x0: str, y0: str, x1: str, y1: str, *,
                  cmap: CMAP_TYPE = default_cmap,
                  alpha: float = 1.0,
                  how: str = "eq_hist",
                  antialias: bool = False,
                  agg=ds.count()) -> GeoPlot:
        self._layers.append(
            LineLayer(df, x0, y0, x1, y1, cmap=cmap, alpha=alpha, how=how, antialias=antialias, agg=agg))
        return self

    def add_grid(self, df, x: str, y: str, *,
                 bins: tuple[int, int] | None = None,
                 cmap: CMAP_TYPE = default_cmap,
                 alpha: float = 1.0,
                 how: str = "eq_hist",
                 agg=ds.count()) -> GeoPlot:
        self._layers.append(GridLayer(df, x, y, bins=bins, cmap=cmap, alpha=alpha, how=how, agg=agg))
        return self

    def add_polygon(self, polygon, *,
                    cmap: CMAP_TYPE = default_cmap,
                    alpha: float = 1.0,
                    how: str = "linear",
                    agg=ds.count()) -> "GeoPlot":
        self._layers.append(PolygonLayer(polygon=polygon, cmap=cmap, alpha=alpha, how=how, agg=agg))
        return self

    def animate(self, *,
                frames=None,
                frame_freq: str | None = None,
                fps: int = 20,
                title=None,
                mode: Literal["exact", "cumulative", "tail"] = "exact",
                show_progress: bool = True):
        from .geoanimate import GeoAnimation

        return GeoAnimation(self,
                            frames=frames,
                            frame_freq=frame_freq,
                            fps=fps,
                            title=title,
                            mode=mode,
                            show_progress=show_progress)

    def _project_point_df(self, layer) -> pd.DataFrame:
        x, y = self._transformer.transform(
            layer.df[layer.x].to_numpy(),
            layer.df[layer.y].to_numpy(),
        )
        pts = pd.DataFrame({"x": x, "y": y})
        pts = pts.replace([np.inf, -np.inf], np.nan).dropna()
        return pts

    def _project_line_df(self, layer: LineLayer) -> pd.DataFrame:
        x0, y0 = self._transformer.transform(layer.df[layer.x0].to_numpy(),
                                             layer.df[layer.y0].to_numpy())
        x1, y1 = self._transformer.transform(layer.df[layer.x1].to_numpy(),
                                             layer.df[layer.y1].to_numpy())

        trips = pd.DataFrame({"x0": x0, "y0": y0, "x1": x1, "y1": y1})
        trips = trips.replace([np.inf, -np.inf], np.nan).dropna()
        return trips

    def _project_polygon_df(self, layer):
        geom = shape(layer.polygon) if isinstance(layer.polygon, dict) else layer.polygon
        geom_3857 = shapely_transform(self._transformer.transform, geom)

        if geom_3857.is_empty:
            return gpd.GeoDataFrame(geometry=[], crs="EPSG:3857")

        return gpd.GeoDataFrame({"value": [1]}, geometry=[geom_3857], crs="EPSG:3857")

    def _project_animated_point_df(self, layer: AnimatedGridLayer, *, is_datetime: bool) -> pd.DataFrame:
        x, y = self._transformer.transform(layer.df[layer.x].to_numpy(),
                                           layer.df[layer.y].to_numpy())

        if is_datetime:
            times = pd.to_datetime(layer.df[layer.time], errors="raise")
        else:
            times = pd.to_numeric(layer.df[layer.time], errors="raise")

        pts = pd.DataFrame({
            "x": x,
            "y": y,
            "__time__": times,
        })

        pts = pts.replace([np.inf, -np.inf], np.nan).dropna(subset=["x", "y", "__time__"])
        pts = pts.sort_values("__time__", kind="stable").reset_index(drop=True)
        return pts

    def _project_animated_line_df(self, layer: AnimatedLineLayer, *, is_datetime: bool) -> pd.DataFrame:
        x0, y0 = self._transformer.transform(layer.df[layer.x0].to_numpy(),
                                             layer.df[layer.y0].to_numpy())
        x1, y1 = self._transformer.transform(layer.df[layer.x1].to_numpy(),
                                             layer.df[layer.y1].to_numpy())

        if is_datetime:
            times = pd.to_datetime(layer.df[layer.time], errors="raise")
        else:
            times = pd.to_numeric(layer.df[layer.time], errors="raise")

        trips = pd.DataFrame({"x0": x0, "y0": y0, "x1": x1, "y1": y1, "__time__": times})

        trips = trips.replace([np.inf, -np.inf], np.nan).dropna(subset=["x0", "y0", "x1", "y1", "__time__"])
        trips = trips.sort_values("__time__", kind="stable").reset_index(drop=True)
        return trips

    def _project_layer(self, layer):
        if isinstance(layer, LineLayer):
            return self._project_line_df(layer)
        if isinstance(layer, GridLayer):
            return self._project_point_df(layer)
        if isinstance(layer, PolygonLayer):
            return self._project_polygon_df(layer)
        raise TypeError(f"Unsupported layer type: {type(layer).__name__}")

    def _compute_extent(self, projected_layers: Iterable[pd.DataFrame]):
        if self.bbox_latlon is not None:
            min_lon, min_lat, max_lon, max_lat = self.bbox_latlon
            xmin, ymin = self._transformer.transform(min_lon, min_lat)
            xmax, ymax = self._transformer.transform(max_lon, max_lat)

            if not np.isfinite([xmin, ymin, xmax, ymax]).all():
                raise ValueError(f"Invalid bbox_latlon: {self.bbox_latlon}")

            if xmin == xmax or ymin == ymax:
                raise ValueError(f"Degenerate bbox_latlon: {self.bbox_latlon}")

            return (xmin, xmax), (ymin, ymax)

        xs = []
        ys = []

        for df in projected_layers:
            if df is None or len(df) == 0:
                continue

            cols = set(df.columns)

            if {"x0", "x1", "y0", "y1"}.issubset(cols):
                x0 = df["x0"].to_numpy()
                x1 = df["x1"].to_numpy()
                y0 = df["y0"].to_numpy()
                y1 = df["y1"].to_numpy()

                xs.append(x0[np.isfinite(x0)])
                xs.append(x1[np.isfinite(x1)])
                ys.append(y0[np.isfinite(y0)])
                ys.append(y1[np.isfinite(y1)])

            elif {"x", "y"}.issubset(cols):
                x = df["x"].to_numpy()
                y = df["y"].to_numpy()

                xs.append(x[np.isfinite(x)])
                ys.append(y[np.isfinite(y)])

            elif "geometry" in cols:
                bounds = np.array(
                    [g.bounds for g in df["geometry"] if g is not None and not g.is_empty],
                    dtype=float,
                )

                if len(bounds):
                    bx = bounds[:, [0, 2]].ravel()
                    by = bounds[:, [1, 3]].ravel()

                    xs.append(bx[np.isfinite(bx)])
                    ys.append(by[np.isfinite(by)])

            else:
                raise ValueError(f"Unsupported projected layer columns: {list(df.columns)}")

        xs = [x for x in xs if len(x)]
        ys = [y for y in ys if len(y)]

        if not xs or not ys:
            raise ValueError(
                "No valid geometries to plot. Add a layer with valid projected data or pass bbox_latlon."
            )

        xs = np.concatenate(xs)
        ys = np.concatenate(ys)

        x_range = tuple(np.quantile(xs, [self.q, 1 - self.q]))
        y_range = tuple(np.quantile(ys, [self.q, 1 - self.q]))

        if not np.isfinite([x_range[0], x_range[1], y_range[0], y_range[1]]).all():
            raise ValueError("Computed non-finite plot extent.")

        if x_range[0] == x_range[1] or y_range[0] == y_range[1]:
            raise ValueError("Computed degenerate plot extent.")

        return x_range, y_range

    @staticmethod
    def _apply_alpha(img: Image.Image, alpha: float) -> Image.Image:
        if alpha >= 1.0:
            return img
        arr = np.array(img, copy=True)
        arr[..., 3] = np.clip(arr[..., 3].astype(np.float32) * alpha, 0, 255).astype(np.uint8)
        return Image.fromarray(arr, mode="RGBA")

    @staticmethod
    def _resolve_cmap(cmap: CMAP_TYPE | None) -> CMAP_TYPE:
        return default_cmap if cmap is None else cmap

    def build_context(self, *,
                      projected_layers: list[pd.DataFrame] | None = None,
                      return_projected: bool = False):
        if projected_layers is None:
            projected_layers = [self._project_layer(layer) for layer in self._layers]

        if not projected_layers:
            raise ValueError(
                "No layers available to determine extent. Add a static layer, pass bbox_latlon, "
                "or provide projected_layers explicitly."
            )

        x_range, y_range = self._compute_extent(projected_layers)

        aspect = (x_range[1] - x_range[0]) / (y_range[1] - y_range[0])
        height = max(1, round(self.width / aspect))

        canvas = ds.Canvas(plot_width=self.width,
                           plot_height=height,
                           x_range=x_range,
                           y_range=y_range)

        grid_canvases = {}
        for layer in self._layers:
            if isinstance(layer, GridLayer) and layer.bins is not None and layer.bins not in grid_canvases:
                grid_canvases[layer.bins] = _make_grid_canvas(layer.bins, x_range, y_range)

        if self.background_rgba is None:
            base_img, labels_img = render_basemap(self.basemap_style,
                                                  x_range,
                                                  y_range,
                                                  (self.width, height))
        else:
            base_img = Image.new("RGBA", (self.width, height), self.background_rgba)
            labels_img = None

        context = RenderContext(width=self.width,
                                height=height,
                                x_range=x_range,
                                y_range=y_range,
                                canvas=canvas,
                                grid_canvases=grid_canvases,
                                base_img=base_img,
                                labels_img=labels_img)

        if return_projected:
            return context, projected_layers
        return context

    def _shade_projected_layer(self, layer, data, context: RenderContext) -> Image.Image:
        if isinstance(layer, (LineLayer, AnimatedLineLayer)):
            agg = context.canvas.line(data,
                                      x=["x0", "x1"],
                                      y=["y0", "y1"],
                                      axis=1,
                                      agg=layer.agg,
                                      antialias=layer.antialias)
        elif isinstance(layer, (GridLayer, AnimatedGridLayer)):
            if layer.bins is None:
                agg = context.canvas.points(data, x="x", y="y", agg=layer.agg)
            else:
                grid_canvas = context.grid_canvases.get(layer.bins)
                if grid_canvas is None:
                    grid_canvas = _make_grid_canvas(layer.bins, context.x_range, context.y_range)
                    context.grid_canvases[layer.bins] = grid_canvas
                agg = grid_canvas.points(data, x="x", y="y", agg=layer.agg)
        elif isinstance(layer, PolygonLayer):
            agg = context.canvas.polygons(data, geometry="geometry", agg=layer.agg)
        else:
            raise TypeError(f"Unsupported layer type: {type(layer).__name__}")

        layer_img = tf.shade(agg, cmap=layer.cmap, how=layer.how).to_pil().convert("RGBA")

        if isinstance(layer, (GridLayer, AnimatedGridLayer)) and layer.bins is not None:
            layer_img = layer_img.resize((context.width, context.height), resample=Image.NEAREST)

        return self._apply_alpha(layer_img, layer.alpha)

    def render(self, *,
               context: RenderContext | None = None,
               projected_layers: list[pd.DataFrame] | None = None,
               include_labels: bool = True) -> Image.Image:
        if context is None and projected_layers is None:
            if not self._layers:
                raise ValueError("No layers added. Use add_lines(...), add_grid(...), or add_polygon(...).")
            context, projected_layers = self.build_context(return_projected=True)
        elif context is None:
            context = self.build_context(projected_layers=projected_layers)
        elif projected_layers is None:
            projected_layers = [self._project_layer(layer) for layer in self._layers]

        final = context.base_img.copy()

        for layer, data in zip(self._layers, projected_layers):
            if len(data) == 0:
                continue
            layer_img = self._shade_projected_layer(layer, data, context)
            final = self.blend(final, layer_img)

        if include_labels and self.labels and context.labels_img is not None:
            final = Image.alpha_composite(final, context.labels_img)

        return final

    def save(self, path: str) -> None:
        self.render().save(path)


__all__ = ["GeoPlot"]
