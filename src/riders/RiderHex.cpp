#include "RiderHex.h"
#include "riders/Rider.h"


RiderHexIndex::RiderHexIndex(size_t n_riders)
        : rider_id_to_hex_id_(n_riders, INVALID_HEX_ID) {
    sim::events.on<&RiderHexIndex::on_rider_updated_eta_pos>(*this);
}

void RiderHexIndex::on_rider_updated_eta_pos(const RiderUpdatedETAPos &event) {
    auto rider_id = event.rider_id;

    auto new_hex = sim::riders[rider_id].eta_hex();
    auto old_hex = rider_id_to_hex_id_[rider_id];
    if (old_hex == new_hex)
        return;

    if (old_hex != INVALID_HEX_ID)
        hex_id_to_riders_.at(old_hex).erase(rider_id);

    hex_id_to_riders_[new_hex].insert(rider_id);
    rider_id_to_hex_id_[rider_id] = new_hex;
}

const RiderHexIndex::rider_set_t &RiderHexIndex::riders_in_hex(hex_id_t hex) const {
    auto it = hex_id_to_riders_.find(hex);
    if (it != hex_id_to_riders_.end())
        return it->second;

    static RiderHexIndex::rider_set_t s{};
    return s;
}
