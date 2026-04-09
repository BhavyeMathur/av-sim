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


def load_results(run):
    output_dir = run["output_dir"]
    requests = pd.read_parquet(run["requests_path"])

    output = pd.read_parquet(f"{output_dir}/requests.parquet")

    fleet_output = pd.read_parquet(f"{output_dir}/fleet.parquet")
    fleet_output.set_index("timestamp", inplace=True)
    fleet_output = fleet_output.add_prefix("n_")

    waypoints_output = pd.read_parquet(f"{output_dir}/waypoints.parquet")

    output = requests.merge(output, left_index=True, right_index=True, validate="1:1")

    all_output = output.copy()
    output = output[output["completed"]]

    output["response_time"] = output["arrived_pickup_at"] - output["created_at"]
    output["fm_time"] = output["arrived_pickup_at"] - output["start_at"]
    output["pickup_time"] = output["pickedup_at"] - output["arrived_pickup_at"]
    output["lm_time"] = output["arrived_drop_at"] - output["pickedup_at"]
    output["drop_time"] = output["completed_at"] - output["arrived_drop_at"]

    vehicle_state_durations = compute_vehicle_state_durations(waypoints_output)

    return all_output, output, fleet_output, waypoints_output, vehicle_state_durations


def compute_requests_stats(all_output, output):
    mean_fm_dist = float(output["fm_dist"].mean())  # km
    mean_fm_time = float(output["fm_time"].mean() / 60)  # mins
    mean_fm_speed = float(mean_fm_dist / (mean_fm_time / 60))  # kmph

    mean_lm_dist = float(output["lm_dist"].mean())  # km
    mean_lm_time = float(output["lm_time"].mean() / 60)  # mins
    mean_lm_speed = float(mean_lm_dist / (mean_lm_time / 60))  # kmph

    return {
        "total": len(all_output),
        "completed": len(output),
        "dropped": len(all_output) - len(output),
        "service_level": 100 * len(output) / len(all_output),  # %

        "fm_dist": mean_fm_dist,
        "fm_time": mean_fm_time,
        "fm_speed": mean_fm_speed,

        "lm_dist": mean_lm_dist,
        "lm_time": mean_lm_time,
        "lm_speed": mean_lm_speed,

        "response_time": float(output["response_time"].mean() / 60),  # mins
        "pickup_time": float(output["pickup_time"].mean() / 60),  # mins
        "drop_time": float(output["drop_time"].mean() / 60),  # mins

        "pax": float(output["pax"].mean()),
    }


def compute_rider_stats(waypoints_output, vehicle_state_durations):
    lifetime = vehicle_state_durations.sum(axis=1).sum()

    output = dict()
    for col in vehicle_state_durations.columns:
        output[f"{col}_pct"] = float(100 * vehicle_state_durations[col].sum() / lifetime)
    return output


def compute_stats(run):
    all_output, output, fleet_output, waypoints_output, vehicle_state_durations = load_results(run)

    stats = compute_requests_stats(all_output, output)
    stats.update(compute_rider_stats(waypoints_output, vehicle_state_durations))

    return stats


def print_stats(output, all_output, stats, rider_stats=None):
    print(f"Completed: {len(output)} / {len(all_output)} or {stats['service_level']:.2f}%")
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


__all__ = ["compute_stats", "compute_vehicle_state_durations"]
