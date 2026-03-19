#include "RiderSource.h"
#include "riders/Rider.h"
#include "routing/H3.h"


AllRidersSource::AllRidersSource() {
    rider_ids_.reserve(sim::riders.size());
    for (size_t i = 0; i < sim::riders.size(); ++i)
        rider_ids_.push_back(i);
}

HexRidersSource::Range HexRidersSource::candidates(coordinate pick_coord) const {
    auto pick_hex = latlon_to_h3(pick_coord);
    const auto &hexes = hexes_in_increasing_radius(pick_hex, max_radius_);
    return {&index_, &hexes};
}
