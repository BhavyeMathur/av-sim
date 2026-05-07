#pragma once

#include "events/EventBus.h"

#include "riders/Rider.h"
#include "riders/RiderBattery.h"
#include "riders/RiderPAX.h"

#include "Request.h"

namespace sim {
    extern EventBus events;

    extern RiderBattery rider_battery;
    extern RiderPAX rider_pax;

    extern std::vector<Request> requests;
}
