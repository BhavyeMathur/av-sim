#pragma once

#include "includes.h"
#include <ringbuffer.h>

enum class RiderState : uint8_t {
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

// contains a waypoint and the time it takes to complete (travel + dwell at)
// the waypoint from the rider's last waypoint
class RiderStep {
public:
    Waypoint waypoint;

    RiderStep(Waypoint waypoint, duration_t travel_time)
            : waypoint(waypoint), travel_time_(travel_time) {}

    // time it takes to complete waypoint is dwell time + travel time
    [[nodiscard]] duration_t duration() const { return waypoint.dwell_s + travel_time_; }

private:
    duration_t travel_time_;
};

// storing other rider member variables (that are not used as frequently)
// in a separate struct for a smaller Rider struct (better cache-friendliness)
class RiderData {
    friend class Rider;

    friend class RiderManager;

public:
    [[nodiscard]] auto pos() const { return pos_; }

private:
    ringbuffer<RiderStep> steps_; // circular buffer of rider waypoints + travel times
    timestamp_t last_commit_at_ = 0; // timestamp of last completed waypoint
    coordinate pos_; // current rider position
};
