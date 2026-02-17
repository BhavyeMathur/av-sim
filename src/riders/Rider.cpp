#include "Rider.h"

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

void Rider::push_waypoint(timestamp_t now, Waypoint wp) {
    if (state_ == State::Dead)
        throw std::runtime_error("cannot push waypoint to dead rider");

    const auto start_t = std::max(now, eta_at_);

    auto duration = wp.dwell_s;
    if (wp.kind != Waypoint::Kind::Wait)
        duration += approx_eta(start_t, eta_pos_, wp.pos);

    steps_.push_back({wp, duration});

    eta_at_ = start_t + duration;
    eta_pos_ = (wp.kind == Waypoint::Kind::Wait) ? eta_pos_ : wp.pos;

    if (!next_scheduled_)
        schedule_next_(now);
}

// schedule the next waypoint (if any) by pushing it to the global events queue
void Rider::schedule_next_(timestamp_t now) {
    assert(!next_scheduled_ && "should not call schedule_next_() if event already scheduled");

    // if there are no more steps to take then mark ourselves as IDLE (or DEAD)
    // and return after setting next_scheduled_ = false;
    if (steps_.empty()) {
        next_scheduled_ = false;
        state_ = (state_ == State::Dead) ? State::Dead : State::Idle;
        return;
    }

    // otherwise we calculate the completion time of the next waypoint
    // by adding dwell_time + actual_eta (movement time)
    // and push this to the global event queue
    const auto &step = steps_.front();

    auto duration = step.waypoint.dwell_s;
    if (step.waypoint.kind != Waypoint::Kind::Wait)
        duration += actual_eta(now, pos_, step.waypoint.pos);

    if (state_ == State::Idle)
        state_ = State::Busy;

    sim::events.push({now + duration, EventType::RiderWaypoint, RiderWaypoint{id_}});
    next_scheduled_ = true;
}

// called when the next rider waypoint is reached
// the rider updates its position and schedules the next waypoint (if any)
void Rider::complete_waypoint_(timestamp_t now) {
    assert(!steps_.empty() && "no waypoints to complete");

    auto step = steps_.front();
    steps_.pop_front();

    // update position if not a waiting waypoint
    if (step.waypoint.kind != Waypoint::Kind::Wait)
        pos_ = step.waypoint.pos;
    last_commit_at_ = now;

    // recalculate the new ETA of all waypoints
    // knowing that this one was completed at 'now'
    recalculate_eta_at_();

    // schedule the next waypoint
    next_scheduled_ = false;
    schedule_next_(now);
}

void Rider::recalculate_eta_at_() {
    auto final_waypoint_at = last_commit_at_;
    for (auto &step: steps_)
        final_waypoint_at += step.approx_duration;

    eta_at_ = final_waypoint_at;
}

void Rider::on_waypoint(const Event &event) {
    auto rider_id = get<RiderWaypoint>(event.payload).rider_id;
    sim::riders[rider_id].complete_waypoint_(event.t);
}

void Rider::on_login(const Event &event) {
    auto rider_id = get<RiderLogin>(event.payload).rider_id;
    sim::riders[rider_id].login(event.t);
}

void Rider::on_logout(const Event &event) {
    auto rider_id = get<RiderLogout>(event.payload).rider_id;
    sim::riders[rider_id].logoff();
}
