#include "RiderSource.h"
#include "routing/grid/Grid.h"
#include "Request.h"


AllRidersSource::AllRidersSource() {
    rider_ids_[0].reserve(sim::n_riders);
    for (size_t i = 0; i < sim::n_riders; ++i)
        rider_ids_[0].push_back(i);
}

CellRidersSource::Range CellRidersSource::candidate_pools(const Request &req) const {
    auto pick_cell = grid::latlon_to_cell(req.pick_coord);
    const auto &cells = grid::cells_in_increasing_radius(pick_cell, max_radius_);
    return {index_, cells};
}
