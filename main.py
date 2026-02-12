import subprocess
import json

import pandas as pd

import loadshare as ls
from loadshare.util import _check_cluster_id, _check_zone_id, _time_to_seconds
from shared.plot import *


def prepare_riders(city, start, end, bad_login_threshold_mins=10):
    riders = ls.RiderDataFrame.from_historical(city, start, end, folder="data_raw/riders")

    riders.filter_bad_logins(threshold_mins=bad_login_threshold_mins)
    riders.set_timestamp_format("offset")
    riders.sort_values("created_at", inplace=True, ignore_index=True)

    riders.to_parquet(f"data_sim/riders/{riders.cache_name()}.parquet")

    return riders


def compute_historical_speeds(pings: ls.PingDataFrame):
    df = pings[pings["completed"]][["created_at", "lm_dist", "lm_time", "fm_dist", "fm_time"]]
    df = df.resample("1500s", on="created_at").sum()

    fm_speed = ((3600 * df["fm_dist"] / df["fm_time"])
                .rolling(window=10, center=True, min_periods=0).mean().ffill().bfill())
    lm_speed = ((3600 * df["lm_dist"] / df["lm_time"])
                .rolling(window=10, center=True, min_periods=0).mean().ffill().bfill())

    fm_speed = fm_speed.clip(1, 45)
    lm_speed = lm_speed.clip(1, 45)

    return fm_speed, lm_speed


def _save_historical_speeds(pings: ls.PingDataFrame):
    fm_speeds, lm_speeds = compute_historical_speeds(pings)

    plot_lines(PlotSeries(fm_speeds, name="FM Speed", color="#2563eb"),
               PlotSeries(lm_speeds, name="LM Speed", color="#60a5fa"),
               title="Speed (kmph)", subtitle=f"{pings.city.name}, {pings.start} to {pings.end}")

    speeds = pd.DataFrame({"fm_speed": fm_speeds / 3600, "lm_speed": lm_speeds / 3600})
    speeds.index.rename("timestamp", inplace=True)
    speeds.index = (speeds.index - pings.start).total_seconds().astype("uint32")
    speeds.to_csv(f"data_sim/speed/{pings.cache_name()}.txt", sep=" ")


def prepare_pings(city, start, end):
    pings = ls.PingDataFrame.from_historical(city, start, end, folder="data_raw/pings")

    pings.fill_lm_dist()
    pings.fill_drop_time()
    pings.fill_ready_time()
    pings.fill_customer_ids()

    assert not pings["lm_dist"].isna().any(), f"Missing LM distances in {pings["lm_dist"].isna().sum()} rows"
    assert not pings["sla_time"].isna().any(), f"Missing SLA time in {pings["sla_time"].isna().sum()} rows"
    assert not pings["predicted_ready_time"].isna().any(), "Missing predicted ready time in ping data"

    pings.sort_values("created_at", inplace=True, ignore_index=True)

    _save_historical_speeds(pings)

    pings.set_timestamp_format("offset")

    pings.compute_pick_time()
    pings.fill_pick_time()

    pings.print_stats()

    # set these to floats because these have NaN values
    # and we drop them before saving anyway
    pings.set_dtypes(start_at=None, fm_time="float", wait_time="float", lm_time="float")

    sim_pings = pings.drop(["start_at", "fm_dist", "fm_time", "lm_time", "wait_time", "completed"], axis=1)
    sim_pings.to_parquet(f"data_sim/pings/{pings.cache_name()}.parquet")

    pings.set_timestamp_format("datetime")
    return pings


def parse_raw_zone_data(zone_file: str, output: str) -> None:
    """
    `zone_file` has the following format:
            zone_id, config_data
                291, {"clusterId": 25}
            ...

    We convert this into a .txt file with:
            zone cluster lat lon
             291      25 0.0 0.0

    lat/lon are currently dummy values, but these can eventually
    be used for repositioning/hotspot calculation, etc.

    Args:
        zone_file: path to the zone_to_cluster.csv file
        output: path to the output .txt file
    """

    def _extract_cluster_from_config(config_data):
        cluster_id = json.loads(config_data)["clusterId"]
        _check_cluster_id(cluster_id)
        return cluster_id

    zones = pd.read_csv(zone_file)
    zones = zones[["zone_id", "config_data"]]
    print(f"Writing {len(zones)} zones to {output}")

    _check_zone_id(zones["zone_id"])

    # parse and write data
    zones["cluster"] = zones["config_data"].apply(_extract_cluster_from_config)
    zones["zone"] = zones["zone_id"]
    zones["lat"] = 0.0  # TODO replace with actual data
    zones["lon"] = 0.0

    zones.to_csv(output, index=False, columns=["zone", "cluster", "lat", "lon"], sep=" ")


def parse_raw_cluster_data(cluster_file: str, output: str) -> None:
    """
    `cluster_file` has the following format:
            id, name, zone_ids
            30, hyd_amee_madh, "471,482"
            ...

    We convert this into a text file with
            cluster_id zone_ids
                    30 471 482
            ...

    Args:
        cluster_file: path to the genesis_zone_clusters.csv file
        output: path to the output .txt file
    """

    assert cluster_file.endswith(".csv"), "Cluster file must be a .csv file."
    assert output.endswith(".txt"), "Output file must be a .txt file."

    cluster_data = pd.read_csv(cluster_file, usecols=["id", "zone_ids"])
    print(f"Writing {len(cluster_data)} clusters to {output}")

    with open(output, "w") as f:
        f.write("cluster_id zone_ids\n")
        for i, cluster_id, zone_ids in cluster_data.itertuples():
            zones = zone_ids.split(",")

            # error checking
            _check_cluster_id(cluster_id)
            if len(zones) != len(set(zones)):
                raise ValueError(f"Duplicate zones found in cluster {cluster_id}: {zone_ids}")
            assert all(map(_check_zone_id, zones))

            # printing and output
            zones = ' '.join(zones)
            if i < 4:
                print(f"\t{cluster_id:>3}: {zones}")
            if i == 4:
                print("\t...")

            f.write(f"{cluster_id} {zones}\n")


def prepare_inputs(city, start, end, bad_login_threshold_mins=2):
    return (
        prepare_pings(city, start, end),
        prepare_riders(city, start, end, bad_login_threshold_mins=bad_login_threshold_mins)
    )


def build_simulator():
    subprocess.run(
        ["cmake", "--build", "cmake-build-release", "--target", "sim", "-j", "8"],
        check=True,
    )


def run_simulation(*inputs):
    build_simulator()

    args = ["./sim"]
    for pings, riders in inputs:
        name = pings.cache_name()
        with open(f"data_sim/sim_configs/{name}.txt", "w") as f:
            f.write(f"start: {_time_to_seconds(pings.start)}\n")
            f.write(f"length: {pings.time_lims.seconds()}\n")
            f.write(f"pings: {name}\n")
            f.write(f"riders: {riders.cache_name()}\n")
            f.write(f"output: {name}\n")
            f.write(f"speed: {name}\n")
            f.write(f"zones: Loadshare\n")
            f.write(f"clusters: Loadshare\n")
            f.write(f"genesis_name: {name}\n")

        args.append(name)

    print(' '.join(args))
    subprocess.run(args, check=True)

    outputs = []
    rider_outputs = []
    stats = pd.DataFrame()

    for (pings, riders) in inputs:
        output = pd.read_parquet(f"output/{pings.cache_name()}.parquet")
        del output["drop_time"]  # TODO remove this line if drop times may vary inside the sim

        sim_pings = pd.read_parquet(f"data_sim/pings/{pings.cache_name()}.parquet")
        output = sim_pings.merge(output, left_index=True, right_index=True,
                                 validate="1:1", how="left", suffixes=("_input", ""))
        output = ls.PingDataFrame(pings.city, pings.start, pings.end, output, timestamp_format="offset")

        output.set_timestamp_format("datetime")
        output.compute_timestamps(drop=False)

        riders.set_timestamp_format("datetime")
        output.compute_prev_data(riders)
        run_stats = pd.DataFrame(output.compute_stats(), index=[0])
        stats = pd.concat([stats, run_stats], ignore_index=True)

        # rider stats

        df = output[output["completed"]]
        rider_stats = df.groupby("rider").agg(total_drop_time=("drop_time", "sum"),
                                              total_fm_time=("fm_time", "sum"),
                                              total_lm_time=("lm_time", "sum"),
                                              total_wait_time=("wait_time", "sum"),
                                              total_orders=("rider", "count"))

        riders = riders.merge(rider_stats, left_index=True, right_on="rider", how="left")
        riders.set_index("rider", inplace=True)
        riders["lifetime"] = (riders["logout_at"] - riders["created_at"]).dt.total_seconds()

        riders = riders.groupby("user_id").agg(total_drop_time=("total_drop_time", "sum"),
                                               total_fm_time=("total_fm_time", "sum"),
                                               total_lm_time=("total_lm_time", "sum"),
                                               total_wait_time=("total_wait_time", "sum"),
                                               total_login_time=("lifetime", "sum"),
                                               delivered=("total_orders", "sum"))

        riders["busy_time"] = riders["total_fm_time"] + riders["total_lm_time"] + riders["total_wait_time"] + riders[
            "total_drop_time"]
        riders["idle_time"] = riders["total_login_time"] - riders["busy_time"]

        riders = riders.astype(int)
        riders["busy_time"] /= riders["total_login_time"]
        riders["idle_time"] /= riders["total_login_time"]

        outputs.append(output)
        rider_outputs.append(riders)

    return outputs, rider_outputs, stats


__all__ = ["prepare_inputs", "run_simulation", "compute_historical_speeds",
           "parse_raw_zone_data", "parse_raw_cluster_data", "build_simulator"]
