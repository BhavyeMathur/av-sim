#pragma once

#include "includes.h"

struct Order;

class RiderPool;

struct AllocationResult {
    std::optional<rider_id_t> rider_id{std::nullopt};
};

class AllocationEngine {
public:
    AllocationEngine();

    AllocationResult match(const Order &order, const RiderPool &riders);

    void update() {
    };

private:
    distance_t fm_cutoff_km;
    speed_t speed_kmps = 40.0 / 3600;

    duration_t pick_time_s = 120;
    duration_t drop_time_s = 120;
};
