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
    auto rider_id = sim::requests[event.request_id].assigned_rider();
    update(rider_id);
}

void RiderGrid::update(rider_id_t rider_id) {
    auto &rider = sim::riders[rider_id];

    auto new_cell = rider.eta_cell();
    if (rider.n_requests_assigned() >= 2)
        new_cell = INVALID_CELL_ID;

    auto old_cell = rider_id_to_cell_[rider_id];
    if (old_cell == new_cell)
        return;

    auto [min_cell, max_cell] = std::minmax(new_cell, old_cell);
    auto min_lock = min_cell == INVALID_CELL_ID ? unique_spinlock() : unique_spinlock(get_lock(min_cell));
    auto max_lock = max_cell == INVALID_CELL_ID ? unique_spinlock() : unique_spinlock(get_lock(max_cell));

    if (old_cell != INVALID_CELL_ID) {
        auto &vec = cell_to_riders_.at(old_cell).riders;
        auto pos = rider_id_to_pos_[rider_id];

        auto moved = vec.back();
        vec[pos] = moved;
        vec.pop_back();

        rider_id_to_pos_[moved] = pos;
    }

    if (new_cell != INVALID_CELL_ID) {
        auto [it, inserted] = cell_to_riders_.try_emplace(new_cell);
        if (inserted)
            it->second.cell = new_cell;

        auto &vec = it->second.riders;
        rider_id_to_pos_[rider_id] = vec.size();
        vec.push_back(rider_id);
    }

    rider_id_to_cell_[rider_id] = new_cell;
}
