#define DEBUG false

#include "RiderBattery.h"
#include "Rider.h"


void RiderBattery::init() {
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

    // charge in-place
    // TODO different charging stations & policies
    rider.push_waypoint({rider.eta_pos(), 4 * 60, INVALID_REQ_ID, Waypoint::Kind::ChargeStart});
    rider.push_waypoint({rider.eta_pos(), charge_time, INVALID_REQ_ID, Waypoint::Kind::ChargeDone});
}

void RiderBattery::on_update_rider_eta_pos_(const RiderUpdatedETAPos &e) {
    assert(rider_id_to_state_[e.rider_id].eta_range_ >= e.distance);
    rider_id_to_state_[e.rider_id].eta_range_ -= e.distance;

    debug("RiderBattery::on_update_rider_eta_pos(rider_id=%i) eta_range_=%f\n", e.rider_id,
          rider_id_to_state_[e.rider_id].eta_range_);
}

void RiderBattery::on_rider_charge_complete_(const RiderChargeComplete &e) {
    auto &state = rider_id_to_state_[e.rider_id];
    state.range_ = state.eta_range_ = capacity_;
}
