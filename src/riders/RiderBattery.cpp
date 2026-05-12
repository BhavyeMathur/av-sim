#define DEBUG false

#include "RiderBattery.h"
#include "Rider.h"

#include "extern.h"


void RiderBattery::init() {
    rider_id_to_state_.resize(sim::n_riders);

    if (sim::configs.policy.charging == "disable")
        return;

    sim::events.on<&RiderBattery::on_update_rider_eta_pos_>(*this);
    sim::events.on<&RiderBattery::on_rider_charge_start_>(*this);
    sim::events.on<&RiderBattery::on_rider_charge_complete_>(*this);
}

bool RiderBattery::check_capacity(rider_id_t rider_id, distance_t distance) const {
    return rider_id_to_state_[rider_id].eta_range_ > distance;
}

const RiderBattery::State &RiderBattery::state(rider_id_t rider_id) const {
    return rider_id_to_state_[rider_id];
}

void RiderBattery::charge(Rider &rider, coordinate at) {
    assert(!sim::rider_mutexes[rider.id()].try_lock());

    auto &state = rider_id_to_state_[rider.id()];
    if (state.charging_scheduled_)
        return;
    state.charging_scheduled_ = true;

    auto approx_charge_time = static_cast<duration_t>(charge_time_ * (state.eta_range_ / capacity_));

    // go to charging location and dummy charging node
    debug("rider(%i).push_waypoint(ChargeStart)\n", rider.id());
    rider.push_waypoints(Waypoint{at, 0, INVALID_REQ_ID, Waypoint::Kind::ChargeStart},
                         Waypoint{at, approx_charge_time, INVALID_REQ_ID, Waypoint::Kind::ChargeDone});

    state.eta_range_ = capacity_;  // we will be back to full capacity once charging is complete
}

// on_update_rider_eta_pos is called when a new Waypoint is created
// and uses an approximate distance function, so we only update the eta_range_
void RiderBattery::on_update_rider_eta_pos_(const RiderUpdatedETAPos &e) {
    assert(rider_id_to_state_[e.rider_id].eta_range_ + 60 >= e.distance);
    rider_id_to_state_[e.rider_id].eta_range_ -= e.distance;
}

// on_rider_schedule waypoint is called when a Waypoint is committed with
// and actual distance, which is why we update the range_
void RiderBattery::on_rider_schedule_waypoint(const RiderScheduleWaypoint &e) {
    assert(rider_id_to_state_[e.rider_id].range_ + 60 >= e.distance);
    rider_id_to_state_[e.rider_id].range_ -= e.distance;
}

void RiderBattery::on_rider_charge_start_(const RiderChargeStart &e) {
    auto &state = rider_id_to_state_[e.rider_id];
    auto &rider = sim::riders[e.rider_id];

    auto charge_time = static_cast<duration_t>(charge_time_ * (state.range_ / capacity_));

    // update the charging waypoint with the correct duration
    auto &next_step = rider.next_waypoint();
    assert(next_step.waypoint.kind == Waypoint::Kind::ChargeDone);
    next_step.waypoint.dwell_s = charge_time;
}

void RiderBattery::on_rider_charge_complete_(const RiderChargeComplete &e) {
    rider_id_to_state_[e.rider_id].range_ = capacity_;
}
