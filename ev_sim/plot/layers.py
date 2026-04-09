from typing import Sequence
from dataclasses import dataclass, field

import numpy as np
import pandas as pd


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
    ys: Sequence[Sequence[float]] | np.ndarray | pd.DataFrame
    labels: Sequence[str] | None = None
    colors: Sequence[str] | None = None
    alpha: float = 1.0
    baseline: str = "zero"


@dataclass
class BarLayer:
    x: Sequence
    y: Sequence
    label: str | None = None
    color: str | None = None
    width: float = 0.8
    alpha: float = 1.0
    axis: int = 0


__all__ = ["LineLayer", "ScatterLayer", "HistLayer", "PieLayer", "StackLayer", "BarLayer"]
