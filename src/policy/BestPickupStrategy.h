#pragma once

#include "Strategy.h"
#include "routing/grid/Grid.h"
#include "routing/Distance.h"


class BoundedH3BestPickupStrategy : public Strategy {
private:
    CellRidersSource riders{4};

    struct RiderInfo : public Strategy::RiderInfo {
        timestamp_t pickup_at = std::numeric_limits<timestamp_t>::max();
    };

    void assign_request(const Request &req) override;

    rider_id_t match(const Request &request);

    static bool is_better(RiderInfo &cand, const RiderInfo &best);
};
