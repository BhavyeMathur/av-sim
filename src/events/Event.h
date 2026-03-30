#pragma once

#include "includes.h"
#include <variant>

struct SimStart {};
struct SimComplete {};

struct RiderPayload {
    rider_id_t rider_id;
};

struct RequestPayload {
    request_id_t request_id;
};

struct RiderLogin : public RiderPayload {};
struct RiderLogout : public RiderPayload {};
struct RiderWaypoint : public RiderPayload {};

struct RiderScheduleWaypoint {
    rider_id_t rider_id;
    distance_t distance;
};

struct RiderUpdatedETAPos {
    rider_id_t rider_id;
    distance_t distance;  // approximated distance from previous ETA pos
};

struct RiderStateChange {
    rider_id_t rider_id;
    _RiderState old_state;
    _RiderState new_state;
};

struct RiderChargeComplete {
    rider_id_t rider_id;
};

struct RiderChargeStart {
    rider_id_t rider_id;
};

struct RequestCreated : public RequestPayload {};
struct RequestCompleted : public RequestPayload {};

struct RequestAssigned {
    request_id_t request_id;
    rider_id_t rider_id;
};

struct FirstMileStart {
    request_id_t request_id;
    distance_t distance;
};

struct ArrivedAtPickup : public RequestPayload {};

struct LastMileStart {
    request_id_t request_id;
    distance_t distance;
};

struct ArrivedAtDrop : public RequestPayload {};

using EventPayload = std::variant<
        SimStart,
        SimComplete,

        RiderLogin,
        RiderLogout,
        RiderScheduleWaypoint,
        RiderWaypoint,
        RiderStateChange,
        RiderUpdatedETAPos,
        RiderChargeStart,
        RiderChargeComplete,

        RequestCreated,
        RequestCompleted,
        RequestAssigned,

        FirstMileStart,
        ArrivedAtPickup,
        LastMileStart,
        ArrivedAtDrop
>;

struct Event {
    timestamp_t t;
    EventPayload payload;

    bool operator<(const Event &other) const {
        return t < other.t;
    }
};
