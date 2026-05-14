#include "RiderGrid.h"

#include "Request.h"
#include "Rider.h"
#include "events/EventBus.h"
#include "events/EventLock.h"
#include "routing/grid/Grid.h"


RiderGrid::RiderGrid(size_t n_riders) {
    cell_to_riders_.emplace(INVALID_CELL_ID, INVALID_CELL_ID);

    rider_id_to_cell_.reserve(n_riders);
    rider_id_to_idx_.reserve(n_riders);

    sim::events.on<&RiderGrid::on_request_completed>(*this);
}

RiderPool &RiderGrid::riders_in_cell(cell_id_t cell) {
    auto [it, _] = const_cast<RiderGrid *>(this)->cell_to_riders_.try_emplace(cell, cell);
    return it->second;
}

Rider &RiderGrid::get_rider(rider_id_t rider_id) {
    auto cell = rider_id_to_cell_[rider_id];
    auto idx = rider_id_to_idx_[rider_id];
    return cell_to_riders_.at(cell).riders[idx];
}

void RiderGrid::emplace(rider_id_t id, coordinate pos, uint8_t pax) {
    auto cell = grid::latlon_to_cell(pos);
    auto &vec = cell_to_riders_[cell].riders;

    rider_id_to_cell_[id] = cell;
    rider_id_to_idx_[id] = vec.size();
    vec.emplace_back(id, pos, pax);
}

void RiderGrid::on_request_completed(const RequestCompleted &event) {
//    sim::events.trigger(RiderUpdatedETAPos{id_, d});
}

Rider &RiderGrid::update(Rider &rider) {
    auto rider_id = rider.id();
    auto new_cell = grid::latlon_to_cell(rider.eta_pos());

    auto old_cell = rider_id_to_cell_[rider_id];
    auto old_idx = rider_id_to_idx_[rider_id];
    auto &old_pool = cell_to_riders_.at(old_cell);

    if (rider.n_requests_assigned() >= 2)
        new_cell = INVALID_CELL_ID;

    if (old_cell == new_cell)
        return rider;

    auto [it, _] = cell_to_riders_.try_emplace(new_cell, new_cell);
    auto &new_pool = it->second;
    rider_id_to_idx_[rider_id] = new_pool.riders.size();
    new_pool.riders.push_back(rider);

    if (old_idx != old_pool.riders.size() - 1) {
        auto &moved = old_pool.riders.back();
        auto moved_id = moved.id();
        old_pool.riders[old_idx] = moved;
        rider_id_to_idx_[moved_id] = old_idx;
    }
    old_pool.riders.pop_back();

    rider_id_to_cell_[rider_id] = new_cell;
    return new_pool.riders.back();
}
