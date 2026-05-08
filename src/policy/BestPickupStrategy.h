#pragma once

#include "Strategy.h"
#include "routing/grid/Grid.h"
#include "routing/Distance.h"


class BestPickupStrategy {
public:
    struct RiderInfo : public Strategy::RiderInfo {
        timestamp_t pickup_at = std::numeric_limits<timestamp_t>::max();

        bool operator<(const RiderInfo &other) const { return pickup_at < other.pickup_at; }

        bool operator>(const RiderInfo &other) const { return pickup_at > other.pickup_at; }
    };

    static constexpr float speed_kmps = 40.0 / 3600;

    static bool is_better(RiderInfo &cand, const RiderInfo &best);
};


class GlobalBestPickupStrategy final : public BestPickupStrategy,
                                       public SequentialStrategy<GlobalBestPickupStrategy> {
public:
    using RiderInfo = BestPickupStrategy::RiderInfo;

    [[nodiscard]] auto candidate_pools(const Request &request) const { return riders.candidate_pools(request); }

private:
    AllRidersSource riders;
};


class GreedyH3BestPickupStrategy final : public BestPickupStrategy,
                                         public SequentialStrategy<GreedyH3BestPickupStrategy> {
public:
    using RiderInfo = BestPickupStrategy::RiderInfo;

    [[nodiscard]] auto candidate_pools(const Request &request) const { return riders.candidate_pools(request); }

    Strategy::Action on_pool_end(const CellRidersSource::pool_t &, const RiderInfo &best, const Request &);

private:
    CellRidersSource riders;
};

class BoundedH3BestPickupStrategy final : public BestPickupStrategy,
                                          public SequentialStrategy<BoundedH3BestPickupStrategy> {
public:
    using RiderInfo = BestPickupStrategy::RiderInfo;

    [[nodiscard]] auto candidate_pools(const Request &request) const { return riders.candidate_pools(request); }

    Strategy::Action on_pool_start(const CellRidersSource::pool_t &pool, const RiderInfo &best, const Request &request);
    
    Strategy::Action on_pool_end(const CellRidersSource::pool_t &, const RiderInfo &best, const Request &);

private:
    CellRidersSource riders{4};
};

class RingedH3BestPickupStrategy final : public BestPickupStrategy,
                                         public SequentialStrategy<RingedH3BestPickupStrategy> {
public:
    using RiderInfo = BestPickupStrategy::RiderInfo;

    [[nodiscard]] auto candidate_pools(const Request &request) const { return riders.candidate_pools(request); }

private:
    CellRidersSource riders{8};
};
