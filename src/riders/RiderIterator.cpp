#include "RiderIterator.h"
#include "RiderManager.h"
#include "Request.h"
#include "routing/grid/Grid.h"


RidersIterator::Range RidersIterator::candidate_pools(const Request &req) {
    auto pick_cell = grid::latlon_to_cell(req.pick_coord);
    const auto &cells = grid::cells_in_increasing_radius(pick_cell, max_radius_);
    return {cells};
}

RiderPool &RidersIterator::Range::iterator::operator*() {
    return sim::riders.riders_in_cell((*cells_)[cell_pos_]);
}
