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

    [[nodiscard]] timestamp_t started_at() const;

    [[nodiscard]] timestamp_t arrived_pickup_at() const;

    [[nodiscard]] timestamp_t pickedup_at() const;

    [[nodiscard]] timestamp_t arrived_drop_at() const;

    [[nodiscard]] timestamp_t completed_at() const;

    void assign_to(rider_id_t rider_id);

    void start_first_mile(distance_t distance);

    void await_pickup();

    void start_last_mile(distance_t distance);

    void await_drop();

    void mark_completed();

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
