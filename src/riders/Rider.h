#pragma once

#include "includes.h"
#include <coordinate.h>
#include <ringbuffer.h>

#include "routing/Routing.h"


struct Waypoint {
    coordinate pos;

    duration_t dwell_s = 0; // optional dwell time (meaningful for Wait, or for Pickup/Dropoff)
    request_id_t request_id; // optional

    enum class Kind : uint8_t {
        FirstMile,
        WaitForPickup,
        LastMile,
        WaitForDropoff,

        RepositionStart,
        RepositionEnd,
        ChargeStart,
        ChargeDone,
    } kind;
};

enum class _RiderState : uint8_t {
    Dead,
    Idle,

    FirstMile,
    PickingUp,
    LastMile,
    DroppingOff,

    Repositioning,
    Charging,

    SIZE
};

class RiderStep {
public:
    Waypoint waypoint;

    RiderStep(Waypoint waypoint, duration_t travel_time)
            : waypoint(waypoint), travel_time_(travel_time) {
    }

    [[nodiscard]] duration_t duration() const {
        return waypoint.dwell_s + travel_time_;
    }

private:
    duration_t travel_time_;
};


class RiderData {
    friend class Rider;

    ringbuffer<RiderStep> steps_;
    timestamp_t last_commit_at_ = 0;

    coordinate pos_;
    cell_id_t eta_cell_{};
};

class Rider {
public:
    friend class RiderBattery;

    using State = _RiderState;
    using Step = RiderStep;

    explicit Rider(coordinate initial_pos, uint8_t pax);

    [[nodiscard]] rider_id_t id() const { return id_; }

    [[nodiscard]] uint8_t n_requests_assigned() const { return n_assigned_; }

    [[nodiscard]] uint8_t capacity() const { return capacity_; }

    [[nodiscard]] bool check_capacity(distance_t distance) const { return eta_range_ > distance; }

    [[nodiscard]] coordinate pos() const { return sim::riders_data[id_].pos_; }

    [[nodiscard]] coordinate eta_pos() const { return eta_pos_; }

    [[nodiscard]] timestamp_t eta_at() const { return eta_at_; }

    [[nodiscard]] cell_id_t eta_cell() const { return sim::riders_data[id_].eta_cell_; }

    [[nodiscard]] State state() { return state_; }

    [[nodiscard]] Step &next_waypoint() const { return sim::riders_data[id_].steps_.front(); }

    void assign_request();

    void push_waypoint(Waypoint wp) { return push_waypoints(wp); }

    template<typename... W> requires (std::same_as<std::decay_t<W>, Waypoint> && ...)

    void push_waypoints(W &&... wp) {
        static_assert(sizeof...(W) > 0);
        distance_t total_distance = 0;
        coordinate last_pos = eta_pos_;

        if (state_ == State::Dead)
            throw std::runtime_error("cannot push waypoint to dead rider");

        auto &data = sim::riders_data[id_];

        auto process = [&](const Waypoint &w) {
            auto [distance, duration] = approx_eta(last_pos, w.pos);
            data.steps_.push_back({w, duration});
            eta_at_ = std::max(sim::clock, eta_at_) + duration + w.dwell_s;

            total_distance += distance;
            last_pos = w.pos;
        };
        (process(wp), ...);

        update_eta_pos_(total_distance, last_pos);
        if (!next_scheduled_)
            schedule_next_(data);
    }

    void complete_waypoint();

    void charge(coordinate at);

    static std::string state_to_string(State state);

private:
    static constexpr distance_t max_range_ = 0.8 * 300;  // 240 km
    static constexpr duration_t charge_time_ = 3600;    // 1 hour
    static rider_id_t next_id_;

    rider_id_t id_;

    coordinate eta_pos_{};
    timestamp_t eta_at_ = 0;

    // charging/range related variables
    distance_t eta_range_ = max_range_;

    State state_ = State::Idle;
    uint8_t n_assigned_ = 0;
    uint8_t capacity_;
    bool next_scheduled_ = false;  // next completion scheduling guard

    void schedule_next_(RiderData &data);

    void recalculate_eta_at_(RiderData &data);

    void update_eta_pos_(distance_t d, coordinate c);

    void set_state_if_not_dead_(State state);
};
