#include "Rider.h"
#include "Request.h"

rider_id_t Rider::next_id_ = 0;

void Rider::login(timestamp_t now) {
    // printf("rider %i logging in\n", id_);
    if (state_ != State::Dead)
        throw std::runtime_error("rider already logged in");

    state_ = State::Idle;
    eta_at_ = last_commit_at_ = now;
    eta_pos_ = pos_;
}

void Rider::logoff() {
    if (state_ == State::Dead)
        throw std::runtime_error("rider already logged off");

    state_ = State::Dead;
    steps_.clear();
}

void Rider::push_waypoint(Waypoint wp) {
    if (state_ == State::Dead)
        throw std::runtime_error("cannot push waypoint to dead rider");

    auto [distance, duration] = approx_eta(eta_pos_, wp.pos);
    duration += wp.dwell_s;
    steps_.push_back({wp, duration});

    eta_at_ = std::max(sim::clock, eta_at_) + duration;
    eta_pos_ = wp.pos;

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
        state_ = (state_ == State::Dead) ? State::Dead : State::Idle;
        return;
    }

    // if there are more steps, rider should be marked busy if they were previously idle
    if (state_ == State::Idle)
        state_ = State::Busy;

    // otherwise we calculate the completion time of the next waypoint
    // by adding dwell_time + actual_eta (movement time)
    // and push this to the global event queue
    const auto &step = steps_.front();

    auto [distance, duration] = actual_eta(pos_, step.waypoint.pos);
    duration += step.waypoint.dwell_s;

    sim::events.push({sim::clock + duration, EventType::RiderWaypoint, RiderWaypoint{id_}});
    next_scheduled_ = true;

    // perform action based on the type of the waypoint
    // at the time when the waypoint is scheduled
    switch (step.waypoint.kind) {
        case Waypoint::Kind::FirstMile:
            sim::requests[step.waypoint.request_id].start_first_mile(distance);
            break;

        case Waypoint::Kind::WaitForPickup:
            sim::requests[step.waypoint.request_id].await_pickup();
            break;

        case Waypoint::Kind::LastMile:
            sim::requests[step.waypoint.request_id].start_last_mile(distance);
            break;

        case Waypoint::Kind::WaitForDropoff:
            sim::requests[step.waypoint.request_id].await_drop();
            break;

        default:
    }
}

// called when the next rider waypoint is reached
// the rider updates its position and schedules the next waypoint (if any)
void Rider::complete_waypoint_() {
    assert(!steps_.empty() && "no waypoints to complete");

    auto step = steps_.front();
    steps_.pop_front();

    // perform action based on the type of the waypoint
    // at the time when the waypoint is completed
    switch (step.waypoint.kind) {
        case Waypoint::Kind::WaitForDropoff:
            sim::requests[step.waypoint.request_id].mark_completed();
            break;

        default:
    }

    // update position and last commit at
    pos_ = step.waypoint.pos;
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

void Rider::on_waypoint(const Event &event) {
    auto rider_id = get<RiderWaypoint>(event.payload).rider_id;
    sim::riders[rider_id].complete_waypoint_();
}

void Rider::on_login(const Event &event) {
    auto rider_id = get<RiderLogin>(event.payload).rider_id;
    sim::riders[rider_id].login(event.t);
}

void Rider::on_logout(const Event &event) {
    auto rider_id = get<RiderLogout>(event.payload).rider_id;
    sim::riders[rider_id].logoff();
}
