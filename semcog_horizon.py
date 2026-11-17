"""Generate a configurable horizon using your existing SEMCOG weekly sampler.

Notebook setup (keep all your current geography and demand adaptations):

1. Add a keyword-only ``rng=None`` argument to ``sample_semcog_data``.
2. Replace its unconditional ``rng = np.random.default_rng(seed=0)`` with:

       if rng is None:
           rng = np.random.default_rng(seed=0)

3. Put this module beside semcog-inputs.ipynb and call:

       from semcog_horizon import sample_semcog_horizon

       HORIZON_S = 28 * 24 * 3600
       raw_trips = sample_semcog_horizon(
           sample_semcog_data, df, gdf, horizon_s=HORIZON_S, seed=0
       )

Continue the existing regional cropping, preprocess_requests, and save cells.
Use a distinct request filename to preserve earlier inputs. Point the experiment
YAML to that saved file and set sim.length_s to HORIZON_S (or a shorter cutoff).

Each week is freshly sampled with one shared random generator. The same seed
gives the same request prefix across horizon lengths, assuming the weekly
sampler uses this generator for every random draw and leaves df/gdf unchanged.
The sampler is evaluated for complete weeks, then the final week is filtered;
this preserves the original overnight and partial-period arrival distributions.
"""

from operator import index

import numpy as np
import pandas as pd


WEEK_S = 7 * 24 * 3600
# The current C++ YAML loader reads sim.length_s as a signed 32-bit integer.
MAX_HORIZON_S = 2**31 - 1


def sample_semcog_horizon(
    sample_week,
    df: pd.DataFrame,
    gdf: pd.DataFrame,
    *,
    horizon_s: int = WEEK_S,
    seed=0,
) -> pd.DataFrame:
    """Return raw requests with 0 <= created_at < horizon_s.

    sample_week must accept (df, gdf, rng=generator), produce one week's raw
    requests, and store integer-second created_at values in [0, WEEK_S).
    Output retains the weekly sampler's columns; created_at remains uint32.
    Raw output is not necessarily sorted. Run the notebook's normal regional
    cropping and preprocess_requests before saving for the C++ simulator.
    """
    if isinstance(horizon_s, (bool, np.bool_)):
        raise ValueError("horizon_s must be a positive integer number of seconds")
    try:
        horizon_s = index(horizon_s)
    except TypeError as exc:
        raise ValueError(
            "horizon_s must be a positive integer number of seconds"
        ) from exc
    if not 1 <= horizon_s <= MAX_HORIZON_S:
        raise ValueError(f"horizon_s must be between 1 and {MAX_HORIZON_S} seconds")

    rng = np.random.default_rng(seed)
    parts = []
    for week in range((horizon_s + WEEK_S - 1) // WEEK_S):
        weekly = sample_week(df, gdf, rng=rng)
        times = weekly["created_at"].to_numpy()
        if not np.issubdtype(times.dtype, np.integer):
            raise ValueError("The weekly sampler must return integer created_at values")
        if np.any(times < 0) or np.any(times >= WEEK_S):
            raise ValueError("The weekly sampler must return timestamps in [0, 604800)")

        # Add offsets before converting back to the simulator's uint32 schema.
        absolute_times = times.astype(np.uint64) + week * WEEK_S
        keep = absolute_times < horizon_s
        part = weekly.loc[keep].copy()
        part["created_at"] = absolute_times[keep].astype(np.uint32)
        parts.append(part)
        # Release the full weekly frame before sampling the next large batch.
        del weekly, times, absolute_times, keep, part

    # Preserve the original raw index for the default single-week call.
    if len(parts) == 1:
        return parts[0]
    return pd.concat(parts, ignore_index=True)


__all__ = ["sample_semcog_horizon", "WEEK_S"]
