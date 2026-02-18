#pragma once

#include "includes.h"
#include <coordinate.h>


class Request {
public:
    request_id_t id;
    timestamp_t created_at;

    coordinate pick_coord;
    coordinate drop_coord;
    distance_t predicted_lm_dist;

    enum class State : uint8_t {
        Unassigned,
        Assigned,    // rider ID has been assigned, first-mile not yet started
        FirstMile,   // rider is moving to pick-up location
        PickingUp,   // rider is picking up request
        LastMile,    // rider is moving to drop-off location
        Dropping,    // rider is dropping off request
        Completed,
    };

    Request(request_id_t id, timestamp_t created_at, coordinate pick_coord, coordinate drop_coord,
            distance_t predicted_lm_dist)
            : id(id),
              created_at(created_at),
              pick_coord(pick_coord),
              drop_coord(drop_coord),
              predicted_lm_dist(predicted_lm_dist) {
    }

    [[nodiscard]] State state() const { return state_; }

    [[nodiscard]] rider_id_t assigned_rider() const {
        _fail_if_not_assigned();
        return rider_;
    }

    [[nodiscard]] distance_t assigned_fm_dist() const {
        _fail_if_not_assigned();
        return fm_dist_;
    }

    [[nodiscard]] distance_t assigned_lm_dist() const {
        _fail_if_not_assigned();
        return lm_dist_;
    }

    [[nodiscard]] timestamp_t assigned_at() const {
        _fail_if_not_assigned();
        return assigned_at_;
    }

    [[nodiscard]] timestamp_t started_at() const {
        if (state_ < State::FirstMile)
            throw std::runtime_error("request has not been started");

        return start_at_;
    }

    [[nodiscard]] timestamp_t arrived_pickup_at() const {
        if (state_ < State::PickingUp)
            throw std::runtime_error("rider has not arrived at pickup yet");

        return arrive_pickup_at_;
    }

    [[nodiscard]] timestamp_t pickedup_at() const {
        if (state_ < State::LastMile)
            throw std::runtime_error("rider has not picked up yet");

        return pickup_at_;
    }

    [[nodiscard]] timestamp_t arrived_drop_at() const {
        if (state_ < State::Dropping)
            throw std::runtime_error("rider has not arrived at drop yet");

        return arrive_drop_at_;
    }

    [[nodiscard]] timestamp_t completed_at() const {
        if (state_ < State::Completed)
            throw std::runtime_error("request has not yet been completed");

        return completed_at_;
    }

    void assign_to(rider_id_t rider_id) {
        assert(state_ == State::Unassigned);
        state_ = State::Assigned;

        rider_ = rider_id;
        assigned_at_ = sim::clock;
    }

    void start_first_mile(distance_t distance) {
        assert(state_ == State::Assigned);
        state_ = State::FirstMile;

        start_at_ = sim::clock;
        fm_dist_ = distance;
    }

    void await_pickup() {
        assert(state_ == State::FirstMile);
        state_ = State::PickingUp;

        arrive_pickup_at_ = sim::clock;
    }

    void start_last_mile(distance_t distance) {
        assert(state_ == State::PickingUp);
        state_ = State::LastMile;

        pickup_at_ = sim::clock;
        lm_dist_ = distance;
    }

    void await_drop() {
        assert(state_ == State::LastMile);
        state_ = State::Dropping;

        arrive_drop_at_ = sim::clock;
    }

    void mark_completed() {
        assert(state_ == State::Dropping);
        state_ = State::Completed;

        completed_at_ = sim::clock;
    }

private:
    rider_id_t rider_ = -1;
    distance_t fm_dist_{};
    distance_t lm_dist_{};

    timestamp_t assigned_at_{};
    timestamp_t start_at_{};
    timestamp_t arrive_pickup_at_{};
    timestamp_t pickup_at_{};
    timestamp_t arrive_drop_at_{};
    timestamp_t completed_at_{};

    State state_ = State::Unassigned;

    void _fail_if_not_assigned() const {
        if (state_ == State::Unassigned)
            throw std::runtime_error("request has not been assigned");
    }
};
