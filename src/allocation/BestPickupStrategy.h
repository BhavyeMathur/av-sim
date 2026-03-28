#pragma once

#include "Strategy.h"

class BestPickupStrategy {
public:
    struct RiderInfo : public BaseStrategy::RiderInfo {
        timestamp_t pickup_at = std::numeric_limits<timestamp_t>::max();
    };

    static constexpr float speed_kmps = 40.0 / 3600;

    static bool is_better(RiderInfo &cand, const RiderInfo &best);
};


class GlobalBestPickupStrategy : public BestPickupStrategy, public SequentialStrategy<GlobalBestPickupStrategy> {
public:
    using RiderInfo = BestPickupStrategy::RiderInfo;

    [[nodiscard]] auto candidate_pools(const Request &request) const { return riders.candidate_pools(request); }

private:
    AllRidersSource riders;
};


class GreedyH3BestPickupStrategy : public BestPickupStrategy, public SequentialStrategy<GreedyH3BestPickupStrategy> {
public:
    using RiderInfo = BestPickupStrategy::RiderInfo;

    [[nodiscard]] auto candidate_pools(const Request &request) const { return riders.candidate_pools(request); }

private:
    HexRidersSource riders;
};
