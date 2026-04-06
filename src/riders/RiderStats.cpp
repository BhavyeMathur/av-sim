#include "RiderStats.h"
#include "extern.h"

#include <pandas.h>


RiderStats::RiderStats() {
    sim::events.on<&RiderStats::on_rider_waypoint_>(*this);
    sim::events.on<&RiderStats::on_sim_complete_>(*this);
}

void RiderStats::on_rider_waypoint_(const RiderWaypoint &e) {
    auto &rider = sim::riders[e.rider_id];
    rider_id_.push_back(e.rider_id);
    timestamp_.push_back(sim::clock);

    lat_.push_back(rider.pos().lat);
    lon_.push_back(rider.pos().lon);
    state_.push_back(static_cast<uint8_t>(rider.state()));
}

void RiderStats::on_sim_complete_(const SimComplete &) {
    printf("...saving rider statistics (count=%zu)\n", timestamp_.size());

    auto table = pd::make_table(pd::col("rider", rider_id_),
                                pd::col("timestamp", timestamp_),
                                pd::col("lat", lat_),
                                pd::col("lon", lon_),
                                pd::col("state", state_));

    auto filepath = sim::configs.sim.output + "waypoints.parquet";
    pd::write_table_to_parquet(table, filepath);
}
