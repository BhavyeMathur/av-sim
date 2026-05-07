#pragma once

#include "Strategy.h"
#include "BestPickupStrategy.h"

class VehicleBatching : public Strategy,
                        public BestPickupStrategy {
public:
    using RiderInfo = BestPickupStrategy::RiderInfo;

    void assign_request(const Request &request) override;

    [[nodiscard]] auto candidate_pools(const Request &request) const { return riders.candidate_pools(request); }

private:
    HexRidersSource riders;
};
