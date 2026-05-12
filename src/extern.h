#pragma once

#include "events/EventBus.h"

#include "riders/Rider.h"
#include "riders/RiderBattery.h"

#include "Request.h"

namespace sim {
    extern EventBus events;
    extern RiderBattery rider_battery;
    extern std::vector<Request> requests;
}
