import pandas as pd
import numpy as np

from .constants import VEHICLE_STATES

import warnings

warnings.filterwarnings("ignore")


def compute_vehicle_state_durations(waypoints: pd.DataFrame) -> pd.DataFrame:
    df = waypoints.copy()

    df["dt"] = df.groupby("rider")["timestamp"].shift(-1) - df["timestamp"]
    df = df.dropna(subset=["dt"])
    return (df.groupby(["rider", "state"])["dt"]
            .sum()
            .unstack(fill_value=0)
            .rename(columns=VEHICLE_STATES))


def compute_request_time_columns(df):
    df["response_time"] = df["arrived_pickup_at"] - df["created_at"]
    df["fm_time"] = df["arrived_pickup_at"] - df["start_at"]
    df["pickup_time"] = df["pickedup_at"] - df["arrived_pickup_at"]
    df["lm_time"] = df["arrived_drop_at"] - df["pickedup_at"]
    df["drop_time"] = df["completed_at"] - df["arrived_drop_at"]


def load_results(run):
    output_dir = run["output_dir"]
    requests = pd.read_parquet(run["requests_path"])

    output = pd.read_parquet(f"{output_dir}/requests.parquet")
    output = requests.merge(output, left_index=True, right_index=True, validate="1:1")

    fleet_output = pd.read_parquet(f"{output_dir}/fleet.parquet")
    fleet_output.set_index("timestamp", inplace=True)
    fleet_output = fleet_output.add_prefix("n_")

    waypoints = pd.read_parquet(f"{output_dir}/waypoints.parquet")
    vehicle_state_durations = compute_vehicle_state_durations(waypoints)

    return output, fleet_output, waypoints, vehicle_state_durations


def compute_request_statistics(requests, completed=None):
    if "fm_time" not in requests.columns:
        compute_request_time_columns(requests)
    if completed is None:
        completed = requests.loc[requests["completed"]]

    metric_names = ["fm_dist", "fm_time", "fm_speed", "lm_dist", "lm_time", "lm_speed", "response_time", "pickup_time",
                    "drop_time", "pax"]

    total = len(requests)
    n_completed = len(completed)
    stats = {
        "total": total,
        "completed": n_completed,
        "dropped": total - n_completed,
        "service_level": 100.0 * n_completed / total if total else np.nan,
    }

    if n_completed == 0:
        for r in ["mean", "std", "q1", "median", "q3"]:
            for m in metric_names:
                stats[f"{r}_{m}"] = np.nan
        return stats

    fm_dist = completed["fm_dist"].to_numpy(dtype=np.float64, copy=False)
    fm_time = completed["fm_time"].to_numpy(dtype=np.float64, copy=False)
    lm_dist = completed["lm_dist"].to_numpy(dtype=np.float64, copy=False)
    lm_time = completed["lm_time"].to_numpy(dtype=np.float64, copy=False)

    with np.errstate(divide="ignore", invalid="ignore"):
        data = np.vstack([
            fm_dist,
            fm_time / 60.0,
            60.0 * fm_dist / fm_time,
            lm_dist,
            lm_time / 60.0,
            60.0 * lm_dist / lm_time,
            completed["response_time"].to_numpy(dtype=np.float64, copy=False) / 60.0,
            completed["pickup_time"].to_numpy(dtype=np.float64, copy=False) / 60.0,
            completed["drop_time"].to_numpy(dtype=np.float64, copy=False) / 60.0,
            completed["pax"].to_numpy(dtype=np.float64, copy=False),
        ])

    means = np.mean(data, axis=1)
    stds = np.std(data, axis=1)
    q1, medians, q3 = np.quantile(data, [0.25, 0.5, 0.75], axis=1)

    for i, name in enumerate(metric_names):
        stats[f"mean_{name}"] = float(means[i])
        stats[f"std_{name}"] = float(stds[i])
        stats[f"q1_{name}"] = float(q1[i])
        stats[f"median_{name}"] = float(medians[i])
        stats[f"q3_{name}"] = float(q3[i])

    return stats


def compute_rider_stats(waypoints, vehicle_state_durations):
    lifetime = vehicle_state_durations.sum(axis=1).sum()

    output = dict()
    for col in vehicle_state_durations.columns:
        output[f"{col}_pct"] = float(100 * vehicle_state_durations[col].sum() / lifetime)
    return output


def compute_stats(run):
    requests, fleet_output, waypoints, vehicle_state_durations = load_results(run)

    stats = compute_request_statistics(requests)
    stats.update(compute_rider_stats(waypoints, vehicle_state_durations))

    return stats


def print_stats(requests=None, completed=None, stats=None, rider_stats=None):
    if stats is None:
        stats = compute_request_statistics(requests, completed)

    print(f"Completed: {stats['completed']} / {stats['total']} or {stats['service_level']:.2f}%")
    print(f"           ({stats['dropped']} dropped)")
    print()
    print(f"Mean FM Distance:   {stats['fm_dist']:.3f} km ({stats['fm_time']:.2f} mins @ {stats['fm_speed']:.2f} kmph)")
    print(f"Mean LM Distance:   {stats['lm_dist']:.3f} km ({stats['lm_time']:.2f} mins @ {stats['lm_speed']:.2f} kmph)")
    print()
    print(f"Mean Response Time: {stats['response_time']:.2f} mins")
    print(f"Mean Pickup Time:   {stats['pickup_time']:.2f} mins")
    print(f"Mean Drop Time:     {stats['drop_time']:.2f} mins")
    print()
    print(f"Mean PAX: {stats['pax']:.2f}")

    if rider_stats:
        print()
        print("-------- rider state distribution")
        print(f"  Idle: {rider_stats['idle_pct']:.2f}%")
        print(f"    FM: {rider_stats['fm_pct']:.2f}%")
        print(f"    LM: {rider_stats['lm_pct']:.2f}%")
        if "charge_pct" in rider_stats:
            print(f"Charge: {rider_stats['charge_pct']:.2f}%")


__all__ = ["compute_stats", "compute_vehicle_state_durations", "compute_request_statistics",
           "compute_request_time_columns", "print_stats"]
