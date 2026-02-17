#pragma once

#include "includes.h"
#include <variant>

struct RiderPayload {
    rider_id_t rider_id;
};

struct RequestPayload {
    request_id_t request_id;
};

struct RiderLogin : public RiderPayload {};
struct RiderLogout : public RiderPayload {};
struct RiderWaypoint : public RiderPayload {};

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

struct ArrivedAtDropoff : public RequestPayload {};

using EventPayload = std::variant<
        RiderLogin,
        RiderLogout,
        RiderWaypoint,

        RequestCreated,
        RequestCompleted,
        RequestAssigned,

        FirstMileStart,
        ArrivedAtPickup,
        LastMileStart,
        ArrivedAtDropoff
>;

struct Event {
    timestamp_t t;
    EventPayload payload;

    bool operator<(const Event &other) const {
        return t < other.t;
    }
};
