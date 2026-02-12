#pragma once

#include "includes.h"
#include "util/event_vector.h"

class RiderPool;

struct RiderEvents {
    struct RiderEvent {
        timestamp_t at;
        rider_id_t id;

        bool operator<(const RiderEvent &other) const {
            return other.at < at;
        }
    };

    static RiderEvents from_riders(RiderPool &riders);

    event_vector<RiderEvent, &RiderEvent::at> logins;
    event_vector<RiderEvent, &RiderEvent::at> logouts;
};
