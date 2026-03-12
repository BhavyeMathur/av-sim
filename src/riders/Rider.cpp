#define DEBUG false

#include "Rider.h"
#include "Request.h"

#include "routing/H3.h"

rider_id_t Rider::next_id_ = 0;

void Rider::login() {
    // printf("rider %i logging in\n", id_);
    if (state_ != State::Dead)
        throw std::runtime_error("assigned_rider already logged in");

    state_ = State::Idle;
    eta_at_ = last_commit_at_ = sim::clock;
    update_eta_pos_(0, pos_);
}

void Rider::logout() {
    if (state_ == State::Dead)
        throw std::runtime_error("assigned_rider already logged out");

    state_ = State::Dead;
    steps_.clear();
}

void Rider::assign_request() {
    n_assigned_++;
    assert(n_assigned_ <= 3 && "rider can be assigned a maximum of three requests at a time");
}

void Rider::push_waypoint(Waypoint wp) {
    if (state_ == State::Dead)
        throw std::runtime_error("cannot push waypoint to dead assigned_rider");

    auto [distance, duration] = approx_eta(eta_pos_, wp.pos);
    duration += wp.dwell_s;
    steps_.push_back({wp, duration});

    eta_at_ = std::max(sim::clock, eta_at_) + duration;
    update_eta_pos_(distance, wp.pos);

    if (!next_scheduled_)
        schedule_next_();
}

// schedule the next waypoint (if any) by pushing it to the global events queue
void Rider::schedule_next_() {
    assert(!next_scheduled_ && "should not call schedule_next_() if event already scheduled");

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

    sim::events.push({sim::clock + duration, RiderWaypoint{id_}});
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

        default:
    }
}

// called when the next rider waypoint is reached
// the rider updates its position and schedules the next waypoint (if any)
void Rider::complete_waypoint() {
    debug("Rider::complete_waypoint(rider_id=%i)", id_);
    assert(!steps_.empty() && "no waypoints to complete");

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

        default:
    }

    // update position and last commit at
    pos_ = waypoint.pos;
    last_commit_at_ = sim::clock;

    // recalculate the new ETA of all waypoints
    // knowing that this one was completed at 'now'
    recalculate_eta_at_();

    // schedule the next waypoint
    next_scheduled_ = false;
    schedule_next_();
}

void Rider::recalculate_eta_at_() {
    auto final_waypoint_at = last_commit_at_;
    for (auto &step: steps_)
        final_waypoint_at += step.approx_duration;

    eta_at_ = final_waypoint_at;
}

void Rider::update_eta_pos_(distance_t d, coordinate c) {
    eta_pos_ = c;
    eta_hex_ = latlon_to_h3(c);

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
