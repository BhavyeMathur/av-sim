from typing import Type, Literal

import os
from datetime import datetime

import pandas as pd
import numpy as np

from . import cities
from .structs import TimeLims


pd.set_option("display.max_columns", None)
pd.set_option("display.expand_frame_repr", False)

TIMESTAMP_FORMAT_T = Literal["offset", "epoch", "datetime"]
COORDINATE_FORMAT_T = Literal["degrees", "radians"]


class DataFrameBase:
    def __init__(self, city: Type[cities.City], start: str, end: str, df: pd.DataFrame, folder: str = None,
                 coordinate_cols: tuple[str, ...] = (),
                 coordinate_format: COORDINATE_FORMAT_T = "radians",
                 timestamp_format: TIMESTAMP_FORMAT_T = "datetime"):
        self._city = city
        self._folder = folder
        self._df = df

        if isinstance(start, str):
            self._start = datetime.fromisoformat(start)
        else:
            self._start = start

        if isinstance(end, str):
            self._end = datetime.fromisoformat(end)
        else:
            self._end = end

        self._coordinate_format = coordinate_format
        self._timestamp_format = timestamp_format

        self._coordinate_cols = coordinate_cols

    def __getattr__(self, item):
        try:
            return object.__getattribute__(self, item)
        except AttributeError:
            pass

        return getattr(self._df, item)

    def __repr__(self) -> str:
        return self._df.__repr__()

    def __getitem__(self, index: str) -> pd.Series:
        return self._df[index]

    def __setitem__(self, index: str, value) -> None:
        self._df[index] = value

    def __len__(self) -> int:
        return len(self._df)

    @staticmethod
    def _load_parquet_range(folder: str, city: Type[cities.City], start: str, end: str) -> pd.DataFrame:
        """
        Iterates through every parquet file in `folder` and returns a dataframe
        containing only the rows that fall within the given time range.

        Files inside `folder` should be named like {city_name}-{}.{start_time}.{end_time}.parquet.
        For example,
            Bangalore-pings.2025-11-01.2025-11-08.parquet

        Args:
            folder: path to input folder
            city:
            start: "YYYY-MM-DD HH:MM" format
            end: "YYYY-MM-DD HH:MM" format
        """
        start_dt = datetime.fromisoformat(start)
        end_dt = datetime.fromisoformat(end)

        print(f"Loading data from: ")

        dataframes = []
        for file in os.listdir(folder):
            if not file.endswith(".parquet"):
                continue
            if not file.startswith(city.name):
                continue

            _, file_start, file_end, _ = file.split(".")
            file_start = datetime.strptime(file_start, "%Y-%m-%d")
            file_end = datetime.strptime(file_end, "%Y-%m-%d")

            # TODO check for files with overlapping time ranges (i.e. duplicate data)
            if file_start >= end_dt or file_end < start_dt.replace(hour=0, minute=0, second=0, microsecond=0):
                continue

            print(f"\t{file}")
            file = os.path.join(folder, file)
            df = pd.read_parquet(file)

            if not pd.api.types.is_datetime64_any_dtype(df["created_at"]):
                df["created_at"] = pd.to_datetime(df["created_at"], unit="s",
                                                  origin=file_start.replace(hour=5, minute=30))

            dataframes.append(df)

        if len(dataframes) == 0:
            raise FileNotFoundError(f"No data found for {start} to {end}")

        dataframes[0] = dataframes[0][dataframes[0]["created_at"] >= pd.to_datetime(start)]
        dataframes[-1] = dataframes[-1][dataframes[-1]["created_at"] < pd.to_datetime(end)]

        result = pd.concat(dataframes, ignore_index=True)
        if len(result) == 0:
            raise FileNotFoundError(f"No data found for {start} to {end}")

        print(f"\tfound {len(result)} rows")
        return result

    def set_coordinate_format(self, format_: COORDINATE_FORMAT_T) -> None:
        if self._coordinate_format == format_:
            return

        if format_ == "degrees":
            func = np.rad2deg
        elif format_ == "radians":
            func = np.deg2rad
        else:
            raise ValueError("invalid coordinate format")

        for col in self._coordinate_cols:
            self._df[col] = func(self._df[col])

        self._coordinate_format = format_

    def set_timestamp_format(self, format_: TIMESTAMP_FORMAT_T) -> None:
        if self._timestamp_format == format_:
            return

        timestamp_cols = tuple(col for col in self._df.columns if col.endswith("_at"))

        if format_ == "datetime":
            origin = None if self._timestamp_format == "epoch" else self._start
            func = lambda col: pd.to_datetime(col, unit="s", origin=origin)

        elif format_ == "offset" and self._timestamp_format == "datetime":
            func = lambda col: (col - self._start).dt.total_seconds()

        elif format_ == "offset" and self._timestamp_format == "epoch":
            func = lambda col: col - self._start.timestamp()

        elif format_ == "epoch" and self._timestamp_format == "datetime":
            func = lambda col: (col - pd.Timestamp(0)).dt.total_seconds()

        elif format_ == "epoch" and self._timestamp_format == "offset":
            func = lambda col: col + self._start.timestamp()

        else:
            raise ValueError("invalid timestamp format")

        for col in timestamp_cols:
            self._df[col] = func(self._df[col])

        if format_ in {"epoch", "offset"}:
            for col in timestamp_cols:
                try:
                    self._df[col] = self._df[col].astype("UInt32")
                except:
                    print(f"Failed to convert {col} to uint32")
                    raise

        self._timestamp_format = format_

    def sample(self, **kwargs) -> None:
        self._df = self._df.sample(**kwargs)

    def cache_name(self) -> str:
        return f"{self._city.name}-{self._start.strftime("%Y.%m.%d.%H.%M")}-{self._end.strftime("%Y.%m.%d.%H.%M")}"

    @property
    def time_lims(self) -> TimeLims:
        return TimeLims(self._start, self._end)

    city = property(lambda self: self._city)
    start = property(lambda self: self._start)
    end = property(lambda self: self._end)
    df = property(lambda self: self._df)

    timestamp_format = property(lambda self: self._timestamp_format)
    coordinate_format = property(lambda self: self._coordinate_format)

__all__ = ["DataFrameBase"]
