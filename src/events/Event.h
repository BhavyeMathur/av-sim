#pragma once

#include "includes.h"
#include <variant>

using event_id_t = uint64_t;

enum class EventType : uint16_t {
    RequestCreated,
    RiderLogin,
    RiderLogout,
    RiderWaypoint,

    COUNT
};

// event payloads
struct OrderCreated {
    size_t order_id;
};

struct RiderPayload {
    size_t rider_id;
};

struct RiderLogin : public RiderPayload {};
struct RiderLogout : public RiderPayload {};
struct RiderWaypoint : public RiderPayload {};

using EventPayload = std::variant<
        OrderCreated,
        RiderLogin,
        RiderLogout,
        RiderWaypoint
>;

struct Event {
    timestamp_t t;
    EventType type;
    EventPayload payload;

    bool operator<(const Event &other) const {
        return t < other.t;
    }
};
