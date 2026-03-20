#pragma once

#include "includes.h"

class RiderStats {
public:
    RiderStats();

private:
    std::vector<rider_id_t> rider_id_;
    std::vector<timestamp_t> timestamp_;
    std::vector<coordinate_t> lat_;
    std::vector<coordinate_t> lon_;
    std::vector<uint8_t> state_;

    void on_rider_waypoint_(const RiderWaypoint &e);

    void on_sim_complete_(const SimComplete &e);
};
