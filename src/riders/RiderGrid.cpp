#include "RiderGrid.h"
#include "extern.h"


RiderGrid::RiderGrid(size_t n_riders)
        : rider_id_to_cell_(n_riders, INVALID_CELL_ID),
          rider_id_to_pos_(n_riders, UINT32_MAX) {
    sim::events.on<&RiderGrid::on_rider_updated_eta_pos>(*this);
    sim::events.on<&RiderGrid::on_request_completed>(*this);
}

void RiderGrid::on_rider_updated_eta_pos(const RiderUpdatedETAPos &event) {
    update(event.rider_id);
}

void RiderGrid::on_request_completed(const RequestCompleted &event) {
    update(sim::requests[event.request_id].assigned_rider());
}

void RiderGrid::update(rider_id_t rider_id) {
    auto &rider = sim::riders[rider_id];

    auto new_cell = rider.eta_cell();
    auto old_cell = rider_id_to_cell_[rider_id];

    if (rider.n_requests_assigned() >= 2)
        new_cell = INVALID_CELL_ID;

    if (old_cell == new_cell)
        return;

    if (old_cell != INVALID_CELL_ID) {
        auto &vec = cell_to_riders_.at(old_cell);

        uint32_t pos = rider_id_to_pos_[rider_id];
        rider_id_t moved = vec.back();

        vec[pos] = moved;
        rider_id_to_pos_[moved] = pos;

        vec.pop_back();
        rider_id_to_pos_[rider_id] = UINT32_MAX;
    }

    if (new_cell != INVALID_CELL_ID) {
        auto &vec = cell_to_riders_[new_cell];

        rider_id_to_pos_[rider_id] = static_cast<uint32_t>(vec.size());
        vec.push_back(rider_id);
    }

    rider_id_to_cell_[rider_id] = new_cell;
}

const RiderGrid::rider_set_t &RiderGrid::riders_in_cell(cell_id_t cell) const {
    auto it = cell_to_riders_.find(cell);
    if (it != cell_to_riders_.end())
        return it->second;

    static RiderGrid::rider_set_t s{};
    return s;
}
