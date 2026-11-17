"""Arrival-cohort performance plots for completed kag924/av-sim runs.

Run from the av-sim repository root in a notebook:

    from av_sim_timeplots import plot_run_time_series
    result = plot_run_time_series(
        'runs/ann-arbor-28days/run_0000', bin_hours=6, rolling_hours=168
    )
    display(result['weekly'])

The matching original input Parquet must still be available at the path stored
in the generated run's config.yaml. Do not regenerate or overwrite that input
between simulation and analysis: output rows have positional request identity.

Service level follows the repository's saved `completed` flag (the C++ writer
actually sets it for assigned requests). This helper checks that flagged rows
have valid pickup/completion timestamps before computing served-request response
times. It cannot certify underlying simulation logic or convergence.

Rolling metrics use sums of requests, served counts, and response seconds;
they never average bin percentages or bin means. Empty denominators give NaN.
Only full moving windows are plotted. Input and output columns are streamed in
batches, so memory usage is approximately independent of the request count.
"""

from pathlib import Path
import math

import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
import pyarrow.parquet as pq
import yaml

DAY_S = 86400
WEEK_S = 7 * DAY_S


def _seconds(hours, name):
    value = float(hours) * 3600
    if not math.isfinite(value) or value <= 0 or not value.is_integer():
        raise ValueError(f'{name} must correspond to a positive whole number of seconds')
    return int(value)


def _ratio(numerator, denominator, factor=1):
    return factor * numerator / denominator.where(denominator > 0)


class _ArrivalReader:
    """Read positional arrival times while validating the whole sorted input."""
    def __init__(self, path, batch_size):
        self.batches = iter(pq.ParquetFile(path).iter_batches(
            columns=['created_at'], batch_size=batch_size))
        self.buffer = np.empty(0, dtype=np.int64)
        self.offset = 0
        self.previous = None

    def _next(self):
        batch = next(self.batches)
        column = batch.column(0)
        if column.null_count:
            raise ValueError('Input created_at contains missing timestamps')
        values = column.to_numpy(zero_copy_only=False)
        if not np.issubdtype(values.dtype, np.integer):
            raise ValueError('Input created_at must contain integer seconds')
        if np.any(values < 0) or np.any(values > np.iinfo(np.uint32).max):
            raise ValueError('Input created_at is outside the simulator timestamp range')
        values = values.astype(np.int64, copy=False)
        if len(values):
            if (self.previous is not None and values[0] < self.previous) or np.any(values[1:] < values[:-1]):
                raise ValueError('Input requests are not sorted by created_at')
            self.previous = int(values[-1])
        return values

    def take(self, n):
        chunks = []
        remaining = n
        while remaining:
            if self.offset == len(self.buffer):
                try:
                    self.buffer = self._next()
                except StopIteration as exc:
                    raise ValueError('Output has more rows than the original request input') from exc
                self.offset = 0
                if not len(self.buffer):
                    continue
            count = min(remaining, len(self.buffer) - self.offset)
            chunks.append(self.buffer[self.offset:self.offset + count])
            self.offset += count
            remaining -= count
        return chunks[0] if len(chunks) == 1 else np.concatenate(chunks)

    def remainder(self):
        if self.offset < len(self.buffer):
            yield self.buffer[self.offset:]
        while True:
            try:
                yield self._next()
            except StopIteration:
                return


def compute_run_time_series(run_dir, *, repo_root='.', bin_hours=6,
                            rolling_hours=168, batch_size=250000):
    """Return bins, weekly summaries, and the actual generated run configuration."""
    root = Path(repo_root).resolve()
    run_dir = Path(run_dir)
    if not run_dir.is_absolute():
        run_dir = root / run_dir
    with (run_dir / 'config.yaml').open() as handle:
        config = yaml.safe_load(handle)
    sim = config['sim']
    horizon = int(sim['length_s'])
    if horizon <= 0:
        raise ValueError('sim.length_s must be positive')
    bin_s = _seconds(bin_hours, 'bin_hours')
    window_s = _seconds(rolling_hours, 'rolling_hours')
    if DAY_S % bin_s:
        raise ValueError('bin_hours must divide 24 hours (for example 1, 3, 6, 12, or 24)')
    if window_s % bin_s:
        raise ValueError('rolling_hours must be an integer multiple of bin_hours')
    if int(batch_size) != batch_size or batch_size <= 0:
        raise ValueError('batch_size must be a positive integer')

    def resolve(path):
        path = Path(path)
        return path if path.is_absolute() else root / path

    input_path = resolve(sim['requests_file'])
    output_path = resolve(sim['output']) / 'requests.parquet'
    n_bins = (horizon + bin_s - 1) // bin_s
    counts = np.zeros(n_bins, dtype=np.int64)
    served = np.zeros(n_bins, dtype=np.int64)
    response_sum = np.zeros(n_bins, dtype=np.float64)
    arrivals = _ArrivalReader(input_path, int(batch_size))
    output = pq.ParquetFile(output_path)
    columns = ['completed', 'arrived_pickup_at', 'completed_at']
    missing = set(columns) - set(output.schema_arrow.names)
    if missing:
        raise ValueError(f'Missing output columns: {sorted(missing)}')
    last_arrival = None
    for batch in output.iter_batches(columns=columns, batch_size=int(batch_size)):
        if not batch.num_rows:
            continue
        if any(batch.column(i).null_count for i in range(len(columns))):
            raise ValueError('Output contains missing completion flags or timestamps')
        created = arrivals.take(batch.num_rows)
        if np.any(created >= horizon):
            raise ValueError('Output rows extend past the arrival cutoff; verify the original input/config')
        flags = batch.column(0).to_numpy(zero_copy_only=False)
        if not np.isin(flags, [False, True]).all():
            raise ValueError('Output completed flags must be Boolean or 0/1')
        flags = flags.astype(bool, copy=False)
        pickup = batch.column(1).to_numpy(zero_copy_only=False).astype(np.int64)
        finished = batch.column(2).to_numpy(zero_copy_only=False).astype(np.int64)
        response = pickup - created  # signed arithmetic avoids uint32 underflow
        invalid = flags & ((response < 0) | (finished < pickup) | (finished <= created))
        if invalid.any():
            raise ValueError('Some served requests have missing/inconsistent pickup or completion times. '
                             'Check that this run completed correctly and uses its original input.')
        bins = created // bin_s
        counts += np.bincount(bins, minlength=n_bins)
        served += np.bincount(bins[flags], minlength=n_bins)
        response_sum += np.bincount(bins[flags], weights=response[flags], minlength=n_bins)
        last_arrival = int(created[-1])

    # There should be exactly one output row for each input arrival below cutoff.
    for remaining in arrivals.remainder():
        if np.any(remaining < horizon):
            raise ValueError('Output is missing admitted requests. Verify the original input, config, and run completion.')

    start = np.arange(n_bins, dtype=np.int64) * bin_s
    end = np.minimum(start + bin_s, horizon)
    bins = pd.DataFrame(dict(start_s=start, end_s=end, end_day=end / DAY_S,
                             requests=counts, served=served, response_sum_s=response_sum))
    bins['service_level_pct'] = _ratio(bins.served, bins.requests, 100)
    bins['mean_response_min'] = _ratio(bins.response_sum_s, bins.served, 1 / 60)
    bins['full_bin'] = (end - start) == bin_s
    window = window_s // bin_s
    rolled = bins[['requests', 'served', 'response_sum_s']].rolling(window, min_periods=window).sum()
    full_window = bins.full_bin.astype(int).rolling(window, min_periods=window).sum().eq(window)
    bins['rolling_service_level_pct'] = _ratio(rolled.served, rolled.requests, 100).where(full_window)
    bins['rolling_mean_response_min'] = _ratio(rolled.response_sum_s, rolled.served, 1 / 60).where(full_window)
    cumul = bins[['requests', 'served', 'response_sum_s']].cumsum()
    bins['cumulative_service_level_pct'] = _ratio(cumul.served, cumul.requests, 100)
    bins['cumulative_mean_response_min'] = _ratio(cumul.response_sum_s, cumul.served, 1 / 60)
    bins['week'] = bins.start_s // WEEK_S + 1

    weekly = bins.groupby('week', sort=True).agg(
        start_s=('start_s', 'min'), end_s=('end_s', 'max'), requests=('requests', 'sum'),
        served=('served', 'sum'), response_sum_s=('response_sum_s', 'sum')).reset_index()
    weekly['service_level_pct'] = _ratio(weekly.served, weekly.requests, 100)
    weekly['mean_response_min'] = _ratio(weekly.response_sum_s, weekly.served, 1 / 60)
    weekly['full_week'] = (weekly.end_s - weekly.start_s) == WEEK_S
    comparable = weekly.full_week & weekly.full_week.shift(1, fill_value=False)
    weekly['change_service_pp'] = weekly.service_level_pct.diff().where(comparable)
    weekly['change_response_min'] = weekly.mean_response_min.diff().where(comparable)
    weekly = weekly[['week', 'requests', 'served', 'service_level_pct', 'mean_response_min',
                     'change_service_pp', 'change_response_min', 'full_week']]
    return dict(bins=bins, weekly=weekly, config=config, run_dir=run_dir,
                input_path=input_path, output_path=output_path, last_arrival_s=last_arrival,
                bin_hours=bin_s / 3600, rolling_hours=window_s / 3600)


def _finish_axes(axes):
    for ax in axes:
        ax.grid(alpha=0.2)
        ax.spines[['top', 'right']].set_visible(False)
    axes[0].set_ylabel('Service level (%)')
    axes[1].set_ylabel('Mean response time (min)\nServed requests only')
    # Keep a useful local scale for convergence while staying within valid bounds.
    lo, hi = axes[0].get_ylim()
    axes[0].set_ylim(max(0, lo), min(100, hi))
    axes[1].set_ylim(bottom=0)


def plot_run_time_series(run_dir, *, repo_root='.', bin_hours=6, rolling_hours=168,
                         include_cumulative=False, save_dir=None, batch_size=250000):
    """Return a dict with bins, weekly, figure, weekly_figure, and run metadata.

    Two figures are created: metrics over elapsed time and overlays by phase of
    the week. Optional save_dir writes performance_over_time.png and
    performance_by_week.png (overwriting those derived images if they exist).
    No source/input/output Parquet files or database records are changed.
    """
    result = compute_run_time_series(run_dir, repo_root=repo_root, bin_hours=bin_hours,
                                    rolling_hours=rolling_hours, batch_size=batch_size)
    data = result['bins']
    horizon_days = result['config']['sim']['length_s'] / DAY_S
    label = f"{result['run_dir'].parent.name} / {result['run_dir'].name}"
    keys = [('service_level_pct', 'rolling_service_level_pct', 'cumulative_service_level_pct'),
            ('mean_response_min', 'rolling_mean_response_min', 'cumulative_mean_response_min')]
    fig, axes = plt.subplots(2, 1, figsize=(11, 7), sharex=True, layout='constrained')
    for ax, (raw, rolling, cumulative) in zip(axes, keys):
       #ax.plot(data.end_day, data[raw], color='0.65', linewidth=1,
       #         label=f'{result["bin_hours"]:g}-hour arrival cohorts')
        ax.plot(data.end_day, data[raw], color='0.65', linewidth=1,
                        )
        ax.plot(data.end_day, data[rolling], color='#087f8c', linewidth=2.3,
                label=f'Trailing {result["rolling_hours"] / 24:g}-day mean')
        if include_cumulative:
            ax.plot(data.end_day, data[cumulative], color='#cc7a00', linestyle='--',
                    linewidth=1.5, label='Cumulative from day 0')
        for day in np.arange(7, horizon_days, 7):
            ax.axvline(day, color='0.8', linewidth=0.8, linestyle=':')
        ax.set_xlim(0, horizon_days)
    axes[0].legend(loc='best', frameon=False)
    axes[1].set_xlabel('Simulation day (request arrival time; day 0 = Monday 00:00)')
    _finish_axes(axes)
    fig.suptitle(f'Performance over time — {label}')

    week_fig, week_axes = plt.subplots(2, 1, figsize=(11, 7), sharex=True, layout='constrained')
    full_weeks = result['weekly'].set_index('week').full_week
    for week, group in data.groupby('week'):
        phase = (group.end_s - (week - 1) * WEEK_S) / DAY_S
        is_full = bool(full_weeks.loc[week])
        for ax, (raw, _, _) in zip(week_axes, keys):
            ax.plot(phase, group[raw], linewidth=1.5, linestyle='-' if is_full else '--',
                    label=f'Week {week}' + ('' if is_full else ' (partial)'))
    week_axes[0].legend(loc='best', frameon=False, ncol=2)
    week_axes[1].set_xticks(range(8), ['Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat', 'Sun', 'Mon'])
    week_axes[1].set_xlim(0, 7)
    week_axes[1].set_xlabel('Time within week (each point represents the preceding arrival cohort)')
    _finish_axes(week_axes)
    week_fig.suptitle(f'Compare the same weekly phase — {label}')
    result.update(figure=fig, weekly_figure=week_fig)
    if save_dir is not None:
        target = Path(save_dir)
        if not target.is_absolute():
            target = Path(repo_root) / target
        target.mkdir(parents=True, exist_ok=True)
        fig.savefig(target / 'performance_over_time.png', dpi=200)
        week_fig.savefig(target / 'performance_by_week.png', dpi=200)
    return result


__all__ = ['compute_run_time_series', 'plot_run_time_series']
