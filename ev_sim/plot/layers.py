from typing import Sequence
from dataclasses import dataclass, field

import datashader as ds
import pandas as pd


@dataclass
class LineLayer:
    df: pd.DataFrame
    x0: str
    y0: str
    x1: str
    y1: str
    cmap: Sequence[str] = (
        "#0b0b0b",
        "#2b0d0a",
        "#5c1a0f",
        "#a33a17",
        "#ff7b2c",
        "#ffd08a",
        "#fff2de",
    )
    alpha: float = 1.0
    how: str = "eq_hist"
    antialias: bool = False
    agg: object = field(default_factory=ds.count)


__all__ = ["LineLayer"]
