#define DEBUG false

#include "RiderBattery.h"
#include "Rider.h"

RiderBattery::RiderBattery() {
    rider_id_to_state_.resize(sim::riders.size());
    sim::events.on<&RiderBattery::on_update_rider_eta_pos_>(*this);
    sim::events.on<&RiderBattery::on_rider_charge_complete_>(*this);
}

bool RiderBattery::check_capacity(rider_id_t rider_id, distance_t distance) const {
    return rider_id_to_state_[rider_id].eta_range_ > distance;
}

void RiderBattery::charge(Rider &rider) const {
    auto &state = rider_id_to_state_[rider.id()];
    if (state.eta_range_ > minimum_)
        return;

    // TODO charge time should be decided at charge_time with state.range_ not with state.eta_range_
    auto charge_time = static_cast<duration_t>(charge_time_ * (state.eta_range_ / capacity_));

    // charge in-place -----------
    // rider.push_waypoint({rider.eta_pos(), 0, INVALID_REQ_ID, Waypoint::Kind::ChargeStart});
    // charge in-place -----------

    // Closest Charging Station -----------
    static const std::vector<coordinate> chargers = {
            {0.737868043395543,  -1.4614804806035193},
            {0.7383711790474249, -1.461022762884997},
            {0.7373253693979914, -1.4617249397298056},
            {0.7374271558957626, -1.4607404343551826},
            {0.7378009581918764, -1.4621699204336844}
    };  // TODO make this an input file/geo file

    duration_t best_time = std::numeric_limits<duration_t>::max();
    size_t best_charger = -1;
    for (size_t i = 0; i < chargers.size(); i++) {
        auto [_, time] = approx_eta(rider.eta_pos(), chargers[i]);

        if (time < best_time) {
            best_time = time;
            best_charger = i;
        }
    }
    rider.push_waypoint({chargers[best_charger], 0, INVALID_REQ_ID, Waypoint::Kind::ChargeStart});

    // Closest Charging Station -----------

    rider.push_waypoint({rider.eta_pos(), charge_time, INVALID_REQ_ID, Waypoint::Kind::ChargeDone});
}

void RiderBattery::on_update_rider_eta_pos_(const RiderUpdatedETAPos &e) {
    assert(rider_id_to_state_[e.rider_id].eta_range_ >= e.distance);
    rider_id_to_state_[e.rider_id].eta_range_ -= e.distance;
}

void RiderBattery::on_rider_charge_complete_(const RiderChargeComplete &e) {
    auto &state = rider_id_to_state_[e.rider_id];
    state.range_ = state.eta_range_ = capacity_;
}
