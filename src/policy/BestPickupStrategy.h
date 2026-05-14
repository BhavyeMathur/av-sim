#pragma once

#include "Strategy.h"
#include "riders/RiderIterator.h"


class BoundedH3BestPickupStrategy : public Strategy {
private:
    RidersIterator riders{4};

    struct RiderInfo : public Strategy::RiderInfo {
        timestamp_t pickup_at = std::numeric_limits<timestamp_t>::max();
    };

    void assign_request(const Request &req) override;

    Rider *match(const Request &request);

    static bool is_better(RiderInfo &cand, const RiderInfo &best);
};
