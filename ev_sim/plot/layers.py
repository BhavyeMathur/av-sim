from dataclasses import dataclass, field

import datashader as ds
import pandas as pd

from .cmaps import CMAP_TYPE, default_cmap


@dataclass
class LineLayer:
    df: pd.DataFrame
    x0: str
    y0: str
    x1: str
    y1: str
    cmap: CMAP_TYPE = default_cmap
    alpha: float = 1.0
    how: str = "eq_hist"
    antialias: bool = False
    agg: object = field(default_factory=ds.count)


@dataclass
class GridLayer:
    df: object
    x: str
    y: str
    bins: tuple[int, int] | None = None
    cmap: CMAP_TYPE = default_cmap
    alpha: float = 1.0
    how: str = "eq_hist"
    agg: object = ds.count()


@dataclass
class PolygonLayer:
    polygon: dict | object
    cmap: CMAP_TYPE = default_cmap
    alpha: float = 1.0
    how: str = "linear"
    agg: object = ds.count()


__all__ = ["LineLayer", "GridLayer", "PolygonLayer"]
