from __future__ import annotations
from typing import Callable, Iterable

from dataclasses import dataclass
from pathlib import Path

import numpy as np
import pandas as pd
from tqdm.auto import tqdm

import imageio.v2 as iio_v2
import imageio.v3 as iio
from PIL import Image, ImageDraw
import datashader as ds

from .geolayers import AnimatedGridLayer, AnimatedLineLayer

FrameTitle = str | Callable[[object], str] | None


@dataclass
class _PreparedAnimatedLayer:
    layer: AnimatedGridLayer | AnimatedLineLayer
    projected: pd.DataFrame
    is_datetime: bool


class GeoAnimation:
    def __init__(self, plot, *,
                 frames: Iterable | None = None,
                 frame_freq: str | None = None,
                 fps: int = 20,
                 title: FrameTitle = None,
                 mode: str = "exact",
                 show_progress: bool = True) -> None:
        self.plot = plot
        self.frames = None if frames is None else pd.Index(frames)
        self.frame_freq = frame_freq
        self.fps = fps
        self.title = title
        self.mode = mode
        self.show_progress = show_progress

        self._layers: list[AnimatedGridLayer | AnimatedLineLayer] = []
        self._prepared_layers: list[_PreparedAnimatedLayer] | None = None
        self._context = None
        self._static_img: Image.Image | None = None
        self._resolved_frames: pd.Index | None = None
        self._is_datetime: bool | None = None

    def add_lines(self, df, x0: str, y0: str, x1: str, y1: str, *,
                  time: str,
                  tail=None,
                  cmap=None,
                  alpha: float = 1.0,
                  how: str = "eq_hist",
                  antialias: bool = False,
                  agg=ds.count()):
        self._layers.append(AnimatedLineLayer(df=df, x0=x0, y0=y0, x1=x1, y1=y1, time=time, tail=tail,
                                              cmap=self.plot._resolve_cmap(cmap), alpha=alpha, how=how,
                                              antialias=antialias, agg=agg))
        return self

    def add_grid(self, df, x: str, y: str, *,
                 time: str,
                 tail=None,
                 bins: tuple[int, int] | None = None,
                 cmap=None,
                 alpha: float = 1.0,
                 how: str = "eq_hist",
                 agg=ds.count()):
        self._layers.append(AnimatedGridLayer(df=df, x=x, y=y, time=time, tail=tail, bins=bins,
                                              cmap=self.plot._resolve_cmap(cmap), alpha=alpha, how=how, agg=agg))
        return self

    @staticmethod
    def _series_is_datetime(series: pd.Series) -> bool:
        return pd.api.types.is_datetime64_any_dtype(series) or pd.api.types.is_datetime64tz_dtype(series)

    def _infer_time_kind(self) -> bool:
        if self.frames is not None and isinstance(self.frames, pd.DatetimeIndex):
            return True

        if self.frame_freq is not None:
            return True

        if not self._layers:
            raise ValueError("No animated layers added.")

        flags = [self._series_is_datetime(layer.df[layer.time]) for layer in self._layers]
        if any(flags) and not all(flags):
            raise TypeError(
                "All animated layers must use compatible time dtypes; do not mix datetime and numeric time columns.")
        return flags[0]

    def _coerce_series(self, series: pd.Series, is_datetime: bool) -> pd.Series:
        if is_datetime:
            return pd.to_datetime(series)
        return pd.to_numeric(series, errors="raise")

    def _coerce_frame_index(self, frames: Iterable, is_datetime: bool) -> pd.Index:
        idx = pd.Index(frames)
        if is_datetime:
            return pd.DatetimeIndex(pd.to_datetime(idx))
        return pd.Index(pd.to_numeric(np.asarray(idx), errors="raise"))

    def _resolve_frames(self) -> pd.Index:
        is_datetime = self._infer_time_kind()
        self._is_datetime = is_datetime

        if self.frames is not None:
            frames = self._coerce_frame_index(self.frames, is_datetime)
            return frames.sort_values()

        if not self._layers:
            raise ValueError("No animated layers added.")

        first = self._coerce_series(self._layers[0].df[self._layers[0].time], is_datetime)

        if self.frame_freq is not None:
            if not is_datetime:
                raise TypeError("frame_freq is only supported for datetime animations.")
            start = first.min()
            end = first.max()
            return pd.date_range(start=start.floor(self.frame_freq),
                                 end=end.ceil(self.frame_freq),
                                 freq=self.frame_freq)

        return pd.DatetimeIndex(np.sort(pd.unique(first))) if is_datetime else pd.Index(np.sort(pd.unique(first)))

    def _normalize_tail(self, tail):
        if tail is None:
            return None
        if self._is_datetime:
            return pd.to_timedelta(tail)
        return tail

    def _prepare(self) -> None:
        if self._prepared_layers is not None:
            return

        if not self._layers:
            raise ValueError("No animated layers added. Use anim.add_grid(...) or anim.add_lines(...).")

        self._resolved_frames = self._resolve_frames()

        prepared = []
        animated_projected = []

        for layer in self._layers:
            if isinstance(layer, AnimatedLineLayer):
                projected = self.plot._project_animated_line_df(layer, is_datetime=self._is_datetime)
            elif isinstance(layer, AnimatedGridLayer):
                projected = self.plot._project_animated_point_df(layer, is_datetime=self._is_datetime)
            else:
                raise TypeError(f"Unsupported animated layer type: {type(layer).__name__}")

            prepared_layer = _PreparedAnimatedLayer(layer=layer,
                                                    projected=projected,
                                                    is_datetime=self._is_datetime)
            prepared.append(prepared_layer)

            if len(projected):
                animated_projected.append(projected)

        static_projected = [self.plot._project_layer(layer) for layer in self.plot._layers]

        extent_layers = [*static_projected, *animated_projected]

        self._context = self.plot.build_context(projected_layers=extent_layers)
        self._static_img = self.plot.render(context=self._context,
                                            projected_layers=static_projected,
                                            include_labels=False)

        self._prepared_layers = prepared

    def _frame_subset(self, prepared: _PreparedAnimatedLayer, frame_value):
        df = prepared.projected
        if len(df) == 0:
            return df

        if prepared.is_datetime:
            times = pd.DatetimeIndex(df["__time__"])
            frame_value = pd.Timestamp(frame_value)
        else:
            times = np.asarray(df["__time__"], dtype=float)
            frame_value = float(frame_value)

        mode = self.mode

        if mode == "exact":
            left = times.searchsorted(frame_value, side="left")
            right = times.searchsorted(frame_value, side="right")
            return df.iloc[left:right]

        if mode == "cumulative":
            right = times.searchsorted(frame_value, side="right")
            return df.iloc[:right]

        if mode == "tail":
            tail = self._normalize_tail(prepared.layer.tail)
            if tail is None:
                raise ValueError("tail mode requires each animated layer to provide tail=...")

            start = frame_value - tail
            left = times.searchsorted(start, side="left")
            right = times.searchsorted(frame_value, side="right")
            return df.iloc[left:right]

        raise ValueError("mode must be one of {'exact', 'cumulative', 'tail'}")

    def _draw_title(self, img: Image.Image, frame_value) -> Image.Image:
        if self.title is None:
            return img

        if callable(self.title):
            text = self.title(frame_value)
        else:
            text = self.title

        if not text:
            return img

        out = img.copy()
        draw = ImageDraw.Draw(out)
        draw.rounded_rectangle((14, 14, 14 + 14 * max(6, len(str(text))), 50), radius=12, fill=(20, 20, 20, 180))
        draw.text((26, 22), str(text), fill=(255, 255, 255, 255))
        return out

    def frame_values(self) -> pd.Index:
        self._prepare()
        return self._resolved_frames

    def render_frame(self, i: int) -> Image.Image:
        self._prepare()

        frame_value = self._resolved_frames[i]
        final = self._static_img.copy()

        for prepared in self._prepared_layers:
            data = self._frame_subset(prepared, frame_value)
            if len(data) == 0:
                continue

            layer_img = self.plot._shade_projected_layer(prepared.layer, data, self._context)
            final = self.plot.blend(final, layer_img)

        if self.plot.labels and self._context.labels_img is not None:
            final = Image.alpha_composite(final, self._context.labels_img)

        final = self._draw_title(final, frame_value)
        return final

    def render_frames(self):
        self._prepare()
        iterator = range(len(self._resolved_frames))
        if self.show_progress:
            iterator = tqdm(iterator, total=len(self._resolved_frames), desc="Rendering frames")
        for i in iterator:
            yield self.render_frame(i)

    def to_numpy(self, i: int) -> np.ndarray:
        return np.array(self.render_frame(i))

    def save_frames(self, directory: str | Path, *, prefix: str = "frame", ext: str = ".png") -> None:
        self._prepare()

        directory = Path(directory)
        directory.mkdir(parents=True, exist_ok=True)

        iterator = range(len(self._resolved_frames))
        if self.show_progress:
            iterator = tqdm(iterator, total=len(self._resolved_frames), desc="Saving frames")

        for i in iterator:
            self.render_frame(i).save(directory / f"{prefix}_{i:05d}{ext}")

    def save(self, path: str | Path, *, fps: int | None = None, quality: int = 8) -> None:
        self._prepare()

        path = Path(path)
        fps = self.fps if fps is None else fps

        if path.suffix not in {".mp4", ".gif"}:
            raise ValueError("Only .mp4 and .gif supported")

        if path.suffix.lower() == ".gif":
            frames = []
            iterator = self.render_frames()
            if self.show_progress:
                iterator = tqdm(iterator, total=len(self._resolved_frames), desc="Rendering GIF")

            for img in iterator:
                frames.append(np.array(img))

            iio.imwrite(path, frames, duration=1000 / fps, loop=0)
            return

        # quality 0–10 (lower = better)
        writer = iio_v2.get_writer(path, fps=fps, codec="libx264", pixelformat="yuv420p", quality=quality,
                                   ffmpeg_params=["-crf", "0", "-pix_fmt", "yuv444p"])

        try:
            iterator = self.render_frames()
            if self.show_progress:
                iterator = tqdm(iterator, total=len(self._resolved_frames), desc="Encoding video")

            for img in iterator:
                frame = np.array(img.convert("RGB"))
                writer.append_data(frame)

        finally:
            writer.close()


__all__ = ["GeoAnimation"]
