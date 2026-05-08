#include "RiderHex.h"
#include "extern.h"


RiderHexIndex::RiderHexIndex(size_t n_riders)
        : rider_id_to_hex_id_(n_riders, INVALID_HEX_ID),
          rider_id_to_pos_(n_riders, UINT32_MAX) {
    sim::events.on<&RiderHexIndex::on_rider_updated_eta_pos>(*this);
    sim::events.on<&RiderHexIndex::on_request_completed>(*this);
}

void RiderHexIndex::on_rider_updated_eta_pos(const RiderUpdatedETAPos &event) {
    update(event.rider_id);
}

void RiderHexIndex::on_request_completed(const RequestCompleted &event) {
    update(sim::requests[event.request_id].assigned_rider());
}

void RiderHexIndex::update(rider_id_t rider_id) {
    auto &rider = sim::riders[rider_id];

    auto new_hex = rider.eta_hex();
    auto old_hex = rider_id_to_hex_id_[rider_id];

    if (rider.n_requests_assigned() >= 2)
        new_hex = INVALID_HEX_ID;

    if (old_hex == new_hex)
        return;

    if (old_hex != INVALID_HEX_ID) {
        auto &vec = hex_id_to_riders_.at(old_hex);

        uint32_t pos = rider_id_to_pos_[rider_id];
        rider_id_t moved = vec.back();

        vec[pos] = moved;
        rider_id_to_pos_[moved] = pos;

        vec.pop_back();
        rider_id_to_pos_[rider_id] = UINT32_MAX;
    }

    if (new_hex != INVALID_HEX_ID) {
        auto &vec = hex_id_to_riders_[new_hex];

        rider_id_to_pos_[rider_id] = static_cast<uint32_t>(vec.size());
        vec.push_back(rider_id);
    }

    rider_id_to_hex_id_[rider_id] = new_hex;
}

const RiderHexIndex::rider_set_t &RiderHexIndex::riders_in_hex(hex_id_t hex) const {
    auto it = hex_id_to_riders_.find(hex);
    if (it != hex_id_to_riders_.end())
        return it->second;

    static RiderHexIndex::rider_set_t s{};
    return s;
}
