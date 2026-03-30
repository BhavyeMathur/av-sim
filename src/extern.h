#pragma once

#include "events/EventBus.h"

#include "riders/Rider.h"
#include "riders/RiderBattery.h"
#include "riders/RiderPAX.h"

#include "Request.h"

namespace sim {
    extern thread_local EventBus events;

    extern thread_local RiderBattery rider_battery;
    extern thread_local RiderPAX rider_pax;

    extern thread_local std::vector<Request> requests;
}
