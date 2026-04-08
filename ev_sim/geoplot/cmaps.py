from typing import Sequence

CMAP_TYPE = Sequence[str]

cmap_yellow = (
    "#0b0b0b",
    "#2b0d0a",
    "#5c1a0f",
    "#a33a17",
    "#ff7b2c",
    "#ffd08a",
    "#fff2de"
)

cmap_blue = (
    "#0b0f1a",
    "#0d1f3a",
    "#174a7a",
    "#2f7fd1",
    "#6bb6ff",
    "#bfe2ff",
    "#eaf6ff",
)

cmap_teal = (
    "#0b1a1a",
    "#0d3a3a",
    "#1a7a7a",
    "#2fd1c3",
    "#6bfff0",
    "#c7fff9",
    "#efffff",
)

default_cmap = cmap_yellow

__all__ = ["cmap_blue", "cmap_teal", "cmap_yellow"]
