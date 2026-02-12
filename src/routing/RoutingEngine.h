#pragma once

#include "Speed.h"

struct AllocationResult;

struct AllocationStatistic;

struct Order;

struct Rider;

namespace sim {
    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, coordinate_t lat2_rad, coordinate_t lon2_rad);

    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, Coordinate p2);

    distance_t distance(Coordinate p1, coordinate_t lat2_rad, coordinate_t lon2_rad);

    distance_t distance(Coordinate p1, Coordinate p2);
}

class RoutingEngine {
public:
    explicit RoutingEngine(const std::string &speed_config_file);

    AllocationStatistic assign_order(const AllocationResult &allocation, Rider &rider, const Order &order) const;

    void weak_assign(Rider &rider, Coordinate to) const;

    void update();

private:
    Speed m_speed;
};
