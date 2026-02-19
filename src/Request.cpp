#define DEBUG false

#include "Request.h"


timestamp_t Request::started_at() const {
    if (state_ < State::FirstMile)
        throw std::runtime_error("request has not been started");

    return start_at_;
}

timestamp_t Request::arrived_pickup_at() const {
    if (state_ < State::PickingUp)
        throw std::runtime_error("rider has not arrived at pickup yet");

    return arrive_pickup_at_;
}

timestamp_t Request::pickedup_at() const {
    if (state_ < State::LastMile)
        throw std::runtime_error("rider has not picked up yet");

    return pickup_at_;
}

timestamp_t Request::arrived_drop_at() const {
    if (state_ < State::Dropping)
        throw std::runtime_error("rider has not arrived at drop yet");

    return arrive_drop_at_;
}

timestamp_t Request::completed_at() const {
    if (state_ < State::Completed)
        throw std::runtime_error("request has not yet been completed");

    return completed_at_;
}

void Request::assign_to(rider_id_t rider_id) {
    debug("Request::assign_to(rider_id=%i) id=%i\n", rider_id, id);
    assert(state_ == State::Unassigned);
    state_ = State::Assigned;

    rider_ = rider_id;
    assigned_at_ = sim::clock;
}

void Request::start_first_mile(distance_t distance) {
    debug("Request::start_first_mile() id=%i\n", id);
    assert(state_ == State::Assigned);
    state_ = State::FirstMile;

    start_at_ = sim::clock;
    fm_dist_ = distance;
}

void Request::await_pickup() {
    debug("Request::await_pickup() id=%i\n", id);
    assert(state_ == State::FirstMile);
    state_ = State::PickingUp;

    arrive_pickup_at_ = sim::clock;
}

void Request::start_last_mile(distance_t distance) {
    debug("Request::start_last_mile() id=%i\n", id);
    assert(state_ == State::PickingUp);
    state_ = State::LastMile;

    pickup_at_ = sim::clock;
    lm_dist_ = distance;
}

void Request::await_drop() {
    debug("Request::await_drop() id=%i\n", id);
    assert(state_ == State::LastMile);
    state_ = State::Dropping;

    arrive_drop_at_ = sim::clock;
}

void Request::mark_completed() {
    debug("Request::mark_completed() id=%i\n", id);
    assert(state_ == State::Dropping);
    state_ = State::Completed;

    completed_at_ = sim::clock;
}
