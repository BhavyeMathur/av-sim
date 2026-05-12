#define DEBUG false

#include "extern.h"
#include "routing/grid/Grid.h"
#include "events/EventLock.h"


rider_id_t Rider::next_id_ = 0;

Rider::Rider(coordinate initial_pos, uint8_t pax)
        : id_(next_id_++), capacity_(pax) {
    sim::riders_data[id_].pos_ = initial_pos;
    update_eta_pos_(0, initial_pos);
}

void Rider::assign_request() {
    n_assigned_++;
    assert(n_assigned_ <= 2 && "rider can be assigned a maximum of two requests at a time");
}

// schedule the next waypoint (if any) by pushing it to the global events queue
void Rider::schedule_next_(RiderData &data) {
    assert(!next_scheduled_ && "should not call schedule_next_() if event already scheduled");

    auto &steps_ = data.steps_;
    auto &pos_ = data.pos_;

    // if there are no more steps to take then mark ourselves as IDLE (or DEAD)
    // and return after setting next_scheduled_ = false;
    if (steps_.empty()) {
        next_scheduled_ = false;
        set_state_if_not_dead_(State::Idle);
        return;
    }

    // otherwise we calculate the completion time of the next waypoint
    // by adding dwell_time + actual_eta (movement time)
    // and push this to the global event queue
    const auto &waypoint = steps_.front().waypoint;

    auto [distance, duration] = actual_eta(pos_, waypoint.pos);
    duration += waypoint.dwell_s;

    sim::events.trigger(RiderScheduleWaypoint{id_, distance});
    EventBus::push({sim::clock + duration, RiderWaypoint{id_}});
    next_scheduled_ = true;

    // perform action based on the type of the waypoint
    // at the time when the waypoint is scheduled
    switch (waypoint.kind) {
        case Waypoint::Kind::FirstMile:
            set_state_if_not_dead_(State::FirstMile);
            sim::events.trigger(FirstMileStart{waypoint.request_id, distance});
            break;

        case Waypoint::Kind::WaitForPickup:
            set_state_if_not_dead_(State::PickingUp);
            sim::events.trigger(ArrivedAtPickup{waypoint.request_id});
            break;

        case Waypoint::Kind::LastMile:
            set_state_if_not_dead_(State::LastMile);
            sim::events.trigger(LastMileStart{waypoint.request_id, distance});
            break;

        case Waypoint::Kind::WaitForDropoff:
            set_state_if_not_dead_(State::DroppingOff);
            sim::events.trigger(ArrivedAtDrop{waypoint.request_id});
            break;

        case Waypoint::Kind::ChargeStart:
        case Waypoint::Kind::ChargeDone:
            set_state_if_not_dead_(State::Charging);
            break;

        default:
    }
}

// called when the next rider waypoint is reached
// the rider updates its position and schedules the next waypoint (if any)
void Rider::complete_waypoint() {
    debug("Rider::complete_waypoint(rider_id=%i)", id_);
    assert(!steps_.empty() && "no waypoints to complete");

    auto &data = sim::riders_data[id_];
    auto &steps_ = data.steps_;
    auto &pos_ = data.pos_;
    auto &last_commit_at_ = data.last_commit_at_;

    auto waypoint = steps_.front().waypoint;
    steps_.pop_front();

    // perform action based on the type of the waypoint
    // at the time when the waypoint is completed
    switch (waypoint.kind) {
        case Waypoint::Kind::WaitForDropoff:
            debug("Rider::complete_waypoint() rider_id=%i – Waypoint::Kind::WaitForDropoff request_id=%i\n",
                  id_, waypoint.request_id);
            assert(n_assigned_ >= 1 && "rider 'n_assigned_' in invalid state");
            n_assigned_--;

            sim::events.trigger(RequestCompleted{waypoint.request_id});
            break;

        case Waypoint::Kind::ChargeStart:
            sim::events.trigger(RiderChargeStart{id_});
            break;

        case Waypoint::Kind::ChargeDone:
            sim::events.trigger(RiderChargeComplete{id_});
            break;

        default:
    }

    // update position and last commit at
    pos_ = waypoint.pos;
    last_commit_at_ = sim::clock;

    // recalculate the new ETA of all waypoints
    // knowing that this one was completed at 'now'
    recalculate_eta_at_(data);

    // schedule the next waypoint
    next_scheduled_ = false;
    schedule_next_(data);
}

void Rider::charge(coordinate at) {
    EventLock<RiderUpdatedETAPos> lock;

    auto [distance, _] = approx_eta(eta_pos_, at);
    eta_range_ -= distance;

    auto approx_charge_time = static_cast<duration_t>(charge_time_ * (eta_range_ / Rider::max_range_));

    // go to charging location
    push_waypoints(Waypoint{at, 0, INVALID_REQ_ID, Waypoint::Kind::ChargeStart},
                   Waypoint{at, approx_charge_time, INVALID_REQ_ID, Waypoint::Kind::ChargeDone});

    eta_range_ = Rider::max_range_;  // we will be back to full capacity once charging is complete
}

void Rider::recalculate_eta_at_(RiderData &data) {
    auto final_waypoint_at = data.last_commit_at_;
    for (auto &step: data.steps_)
        final_waypoint_at += step.duration();

    eta_at_ = final_waypoint_at;
}

void Rider::update_eta_pos_(distance_t d, coordinate c) {
    assert(eta_range_ >= d);

    auto &data = sim::riders_data[id_];

    eta_pos_ = c;
    eta_range_ -= d;
    data.eta_cell_ = grid::latlon_to_cell(c);

    sim::events.trigger(RiderUpdatedETAPos{id_, d});
}

void Rider::set_state_if_not_dead_(State state) {
    if (state_ == State::Dead)
        return;

    sim::events.trigger(RiderStateChange{id_, state_, state});
    state_ = state;
}

std::string Rider::state_to_string(Rider::State state) {
    switch (state) {
        case State::Dead:
            return "dead";
        case State::Idle:
            return "idle";

        case State::FirstMile:
            return "fm";
        case State::PickingUp:
            return "wait";
        case State::LastMile:
            return "lm";
        case State::DroppingOff:
            return "drop";

        case State::Repositioning:
            return "service";
        case State::Charging:
            return "charge";

        case State::SIZE:
            throw std::invalid_argument("invalid state");
    }
}
