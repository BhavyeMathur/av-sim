#include "RiderSource.h"
#include "routing/H3.h"
#include "Request.h"


AllRidersSource::AllRidersSource() {
    rider_ids_[0].reserve(sim::riders.size());
    for (size_t i = 0; i < sim::riders.size(); ++i)
        rider_ids_[0].push_back(i);
}

HexRidersSource::Range HexRidersSource::candidate_pools(const Request &req) const {
    auto pick_hex = latlon_to_h3(req.pick_coord);
    const auto &hexes = hexes_in_increasing_radius(pick_hex, max_radius_);
    return {index_, hexes};
}
