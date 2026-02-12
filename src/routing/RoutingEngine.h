#pragma once

#include "Order.h"
#include <coordinate.h>

struct AllocationResult;

struct AllocationStatistic;

struct Rider;

namespace sim {
    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, coordinate_t lat2_rad, coordinate_t lon2_rad);

    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, coordinate p2);

    distance_t distance(coordinate p1, coordinate_t lat2_rad, coordinate_t lon2_rad);

    distance_t distance(coordinate p1, coordinate p2);
}

class RoutingEngine {
public:
    AllocationStatistic assign_order(const AllocationResult &allocation, Rider &rider, const Order &order) const;

    void weak_assign(Rider &rider, coordinate to) const;

private:
    speed_t m_speed = 40.0 / 3600;

    duration_t pick_time_s = 120;
    duration_t drop_time_s = 120;
};
