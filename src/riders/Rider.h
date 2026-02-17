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
        Arrive,       // arrive at pos
        Pickup,      // pickup action at pos
        Dropoff,     // dropoff request_id at pos
        Wait,        // dwell at current pos for dwell_s
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

    [[nodiscard]] bool is_alive() const { return state_ != State::Dead; }

    [[nodiscard]] bool is_idle() const { return state_ == State::Idle; }

    [[nodiscard]] coordinate pos() const { return pos_; }

    [[nodiscard]] coordinate eta_pos() const { return eta_pos_; }

    [[nodiscard]] timestamp_t eta_at() const { return eta_at_; }

    void login(timestamp_t now) {
        if (state_ != State::Dead)
            throw std::runtime_error("rider already logged in");

        state_ = State::Idle;
        eta_at_ = last_commit_at_ = now;
        eta_pos_ = pos_;
    }

    void logoff() {
        if (state_ == State::Dead)
            throw std::runtime_error("rider already logged off");

        state_ = State::Dead;
        steps_.clear();
    }

    template<class SpanLike>
        void append_plan(timestamp_t now, const SpanLike &wps) {
            for (const auto &wp: wps)
                push_waypoint(now, wp);
        }

    void push_waypoint(timestamp_t now, Waypoint wp) {
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

    // schedule the next waypoint (if any) by pushing it to the global events queue
    void schedule_next_(timestamp_t now) {
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
    void complete_waypoint_(timestamp_t now) {
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

    void recalculate_eta_at_() {
        auto final_waypoint_at = last_commit_at_;
        for (auto &step: steps_)
            final_waypoint_at += step.approx_duration;

        eta_at_ = final_waypoint_at;
    }

public:
    static void on_waypoint(const Event &event);

    static void on_login(const Event &event);

    static void on_logout(const Event &event);
};
