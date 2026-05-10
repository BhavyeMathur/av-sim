#pragma once

#include "Strategy.h"
#include "BestPickupStrategy.h"

class VehicleBatching : public Strategy {
public:
    struct RiderInfo : public Strategy::RiderInfo {
        timestamp_t pickup_at = std::numeric_limits<timestamp_t>::max();

        bool operator<(const RiderInfo &other) const { return pickup_at < other.pickup_at; }

        bool operator>(const RiderInfo &other) const { return pickup_at > other.pickup_at; }
    };

    void assign_request(const Request &request) override;

    [[nodiscard]] auto candidate_pools(const Request &request) const { return riders.candidate_pools(request); }

private:
    CellRidersSource riders;
};
