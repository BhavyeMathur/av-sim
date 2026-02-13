#pragma once

#include "includes.h"
#include <variant>

using event_id_t = uint64_t;

enum class EventType : uint16_t {
    OrderCreated,
    RiderLogin,
    RiderLogout
};

// event payloads
struct EventOrderCreated {
    size_t order_id;
};

struct EventRiderLogin {
    size_t rider_id;
};

struct EventRiderLogout {
    size_t rider_id;
};

using EventPayload = std::variant<
        EventOrderCreated,
        EventRiderLogin,
        EventRiderLogout
>;

struct Event {
    timestamp_t t;
    EventType type;
    EventPayload payload;
};
