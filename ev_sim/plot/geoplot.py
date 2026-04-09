from __future__ import annotations
from typing import Iterable

import numpy as np
import pandas as pd
import geopandas as gpd

import datashader as ds
import datashader.transfer_functions as tf
from PIL import Image
from pyproj import Transformer

from shapely.geometry import shape
from shapely.ops import transform as shapely_transform

from .basemap import *
from .geolayers import *
from .blend import BLEND_FUNCS
from .cmaps import CMAP_TYPE, default_cmap


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
        """
        Parameters
        ----------
        width
            Output width in pixels.
        bbox_latlon
            Optional fixed extent as (min_lon, min_lat, max_lon, max_lat).
            If omitted, computed from all added layers using quantiles.
        q
            Quantile crop used only when bbox_latlon is None.
        labels
            Whether to overlay label tiles.
        basemap_style
            Tile style settings.
        background_rgba
            If provided, renders on this solid background instead of fetching a basemap.
        """

        self.width = width
        self.bbox_latlon = bbox_latlon
        self.q = q
        self.labels = labels
        self.basemap_style = basemap_style or BasemapStyle()
        self.background_rgba = background_rgba
        self.blend = BLEND_FUNCS[blend] if isinstance(blend, str) else blend

        self._layers: list = []
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

    def _compute_extent(self, projected_layers: Iterable[pd.DataFrame]):
        if self.bbox_latlon is not None:
            min_lon, min_lat, max_lon, max_lat = self.bbox_latlon
            xmin, ymin = self._transformer.transform(min_lon, min_lat)
            xmax, ymax = self._transformer.transform(max_lon, max_lat)
            return (xmin, xmax), (ymin, ymax)

        xs = []
        ys = []

        for df in projected_layers:
            if len(df) == 0:
                continue

            cols = set(df.columns)

            if {"x0", "x1", "y0", "y1"}.issubset(cols):
                xs.append(df["x0"].to_numpy())
                xs.append(df["x1"].to_numpy())
                ys.append(df["y0"].to_numpy())
                ys.append(df["y1"].to_numpy())

            elif {"x", "y"}.issubset(cols):
                xs.append(df["x"].to_numpy())
                ys.append(df["y"].to_numpy())

            elif "geometry" in cols:
                bounds = np.array([g.bounds for g in df["geometry"] if g is not None and not g.is_empty])
                if len(bounds):
                    xs.append(bounds[:, [0, 2]].ravel())
                    ys.append(bounds[:, [1, 3]].ravel())

            else:
                raise ValueError(f"Unsupported projected layer columns: {list(df.columns)}")

        if not xs or not ys:
            raise ValueError("No valid geometries to plot.")

        xs = np.concatenate(xs)
        ys = np.concatenate(ys)

        x_range = tuple(np.quantile(xs, [self.q, 1 - self.q]))
        y_range = tuple(np.quantile(ys, [self.q, 1 - self.q]))
        return x_range, y_range

    @staticmethod
    def _apply_alpha(img: Image.Image, alpha: float) -> Image.Image:
        if alpha >= 1.0:
            return img
        arr = np.array(img, copy=True)
        arr[..., 3] = np.clip(arr[..., 3].astype(np.float32) * alpha, 0, 255).astype(np.uint8)
        return Image.fromarray(arr, mode="RGBA")

    def render(self) -> Image.Image:
        if not self._layers:
            raise ValueError("No layers added. Use add_lines(...), add_hexbin(...), or add_grid(...).")

        projected = []
        for layer in self._layers:
            if isinstance(layer, LineLayer):
                projected.append(self._project_line_df(layer))
            elif isinstance(layer, GridLayer):
                projected.append(self._project_point_df(layer))
            elif isinstance(layer, PolygonLayer):
                projected.append(self._project_polygon_df(layer))
            else:
                raise TypeError(f"Unsupported layer type: {type(layer).__name__}")

        x_range, y_range = self._compute_extent(projected)

        aspect = (x_range[1] - x_range[0]) / (y_range[1] - y_range[0])
        height = max(1, round(self.width / aspect))

        canvas = ds.Canvas(
            plot_width=self.width,
            plot_height=height,
            x_range=x_range,
            y_range=y_range,
        )

        base_img, labels_img = render_basemap(self.basemap_style, x_range, y_range, (self.width, height))
        final = base_img.copy()

        for layer, data in zip(self._layers, projected):
            if len(data) == 0:
                continue

            if isinstance(layer, LineLayer):
                agg = canvas.line(
                    data,
                    x=["x0", "x1"],
                    y=["y0", "y1"],
                    axis=1,
                    agg=layer.agg,
                    antialias=layer.antialias,
                )
            elif isinstance(layer, GridLayer):
                if layer.bins is None:
                    agg = canvas.points(data, x="x", y="y", agg=layer.agg)
                else:
                    nx, ny = layer.bins
                    grid_canvas = ds.Canvas(plot_width=nx, plot_height=ny, x_range=x_range, y_range=y_range)
                    agg = grid_canvas.points(data, x="x", y="y", agg=layer.agg)
            elif isinstance(layer, PolygonLayer):
                agg = canvas.polygons(data, geometry="geometry", agg=layer.agg)
            else:
                raise TypeError(f"Unsupported layer type: {type(layer).__name__}")

            layer_img = tf.shade(agg, cmap=layer.cmap, how=layer.how).to_pil().convert("RGBA")

            if isinstance(layer, GridLayer) and layer.bins is not None:
                layer_img = layer_img.resize((self.width, height), resample=Image.NEAREST)

            layer_img = self._apply_alpha(layer_img, layer.alpha)
            final = self.blend(final, layer_img)

        if self.labels and labels_img is not None:
            final = Image.alpha_composite(final, labels_img)

        return final

    def save(self, path: str) -> None:
        self.render().save(path)


__all__ = ["GeoPlot"]
