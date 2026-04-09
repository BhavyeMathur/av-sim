import pandas as pd

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
    output = compute_vehicle_state_durations(output)

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
        completed = requests[requests["completed"]]

    mean_fm_dist = float(completed["fm_dist"].mean())  # km
    mean_fm_time = float(completed["fm_time"].mean() / 60)  # mins
    mean_fm_speed = float(mean_fm_dist / (mean_fm_time / 60))  # kmph

    mean_lm_dist = float(completed["lm_dist"].mean())  # km
    mean_lm_time = float(completed["lm_time"].mean() / 60)  # mins
    mean_lm_speed = float(mean_lm_dist / (mean_lm_time / 60))  # kmph

    return {
        "total": len(requests),
        "completed": len(completed),
        "dropped": len(requests) - len(completed),
        "service_level": 100 * len(completed) / len(requests),  # %

        "fm_dist": mean_fm_dist,
        "fm_time": mean_fm_time,
        "fm_speed": mean_fm_speed,

        "lm_dist": mean_lm_dist,
        "lm_time": mean_lm_time,
        "lm_speed": mean_lm_speed,

        "response_time": float(completed["response_time"].mean() / 60),  # mins
        "pickup_time": float(completed["pickup_time"].mean() / 60),  # mins
        "drop_time": float(completed["drop_time"].mean() / 60),  # mins

        "pax": float(completed["pax"].mean()),
    }


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
