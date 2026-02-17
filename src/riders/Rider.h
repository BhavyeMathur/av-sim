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


class Rider {
public:
    explicit Rider(coordinate initial_pos)
            : id_(next_id_++),
              pos_(initial_pos),
              eta_pos_(initial_pos) {}

    [[nodiscard]] rider_id_t id() const { return id_; }

    [[nodiscard]] bool is_alive() const { return state_ != State::Dead; }

    [[nodiscard]] bool is_idle() const { return state_ == State::Idle; }

    [[nodiscard]] coordinate pos() const { return pos_; }

    [[nodiscard]] coordinate eta_pos() const { return eta_pos_; }

    [[nodiscard]] timestamp_t eta_at() const { return eta_at_; }

    void login();

    void logout();

    template<class SpanLike>
        void append_plan(const SpanLike &wps) {
            for (const auto &wp: wps)
                push_waypoint(wp);
        }

    void push_waypoint(Waypoint wp);

    void complete_waypoint();

private:
    struct Step {
        Waypoint waypoint;
        duration_t approx_duration; // dwell + approx travel duration
    };

    enum class State : uint8_t {
        Dead,
        Idle,
        Busy
    };

    static rider_id_t next_id_;
    rider_id_t id_;

    ringbuffer<Step> steps_;

    // committed steps/waypoints
    // these are waypoints that are harder to cancel and are already in the event queue
    coordinate pos_;
    timestamp_t last_commit_at_ = 0;

    // tail estimates (approximately estimated)
    coordinate eta_pos_;
    timestamp_t eta_at_ = 0;

    // next completion scheduling guard
    bool next_scheduled_ = false;

    State state_ = State::Dead;

    void schedule_next_();

    void recalculate_eta_at_();
};
