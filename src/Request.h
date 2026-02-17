#pragma once

#include "includes.h"
#include <coordinate.h>


struct Request {
    request_id_t id;
    timestamp_t created_at;

    coordinate pick_coord;
    coordinate drop_coord;
    distance_t predicted_lm_dist;

    Request(request_id_t id, timestamp_t created_at, coordinate pick_coord, coordinate drop_coord,
            distance_t predicted_lm_dist)
            : id(id),
              created_at(created_at),
              pick_coord(pick_coord),
              drop_coord(drop_coord),
              predicted_lm_dist(predicted_lm_dist) {
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

    enum class State : uint8_t {
        Unassigned,
        Assigned,    // rider ID has been assigned, first-mile not yet started
        FirstMile,   // rider is moving to pick-up location
        PickingUp,   // rider is picking up request
        LastMile,    // rider is moving to drop-off location
        Dropping,    // rider is dropping off request
        Completed,
    } state_ = State::Unassigned;
};
