from dataclasses import dataclass
from math import ceil
from typing import Literal

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
        # at most max_rows rows
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


__all__ = ["LegendConfig", "HistogramConfig"]
