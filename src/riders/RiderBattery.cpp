#define DEBUG false

#include "RiderBattery.h"
#include "Rider.h"

#include "events/EventLock.h"


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
    EventLock<RiderUpdatedETAPos> lock;
    auto &state = rider_id_to_state_[rider.id()];

    // go to charging location
    rider.push_waypoint({at, 0, INVALID_REQ_ID, Waypoint::Kind::ChargeStart});

    // since we have created a lock above, we need to manually call on_update_rider_eta_pos
    // to update the eta_range_ and properly calculate the approximate charging time
    auto [distance, _] = approx_eta(rider.eta_pos(), at);
    on_update_rider_eta_pos_({rider.id(), distance});

    // insert dummy waypoint whose duration will be updated once charge start
    auto approx_charge_time = static_cast<duration_t>(charge_time_ * (state.eta_range_ / capacity_));
    rider.push_waypoint({at, approx_charge_time, INVALID_REQ_ID, Waypoint::Kind::ChargeDone});

    state.eta_range_ = capacity_;  // we will be back to full capacity once charging is complete
}

// on_update_rider_eta_pos is called when a new Waypoint is created
// and uses an approximate distance function, so we only update the eta_range_
void RiderBattery::on_update_rider_eta_pos_(const RiderUpdatedETAPos &e) {
    assert(rider_id_to_state_[e.rider_id].eta_range_ >= e.distance);
    rider_id_to_state_[e.rider_id].eta_range_ -= e.distance;
}

// on_rider_schedule waypoint is called when a Waypoint is committed with
// and actual distance, which is why we update the range_
void RiderBattery::on_rider_schedule_waypoint(const RiderScheduleWaypoint &e) {
    assert(rider_id_to_state_[e.rider_id].range_ >= e.distance);
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
