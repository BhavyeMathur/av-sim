#pragma once

#include "events/EventBus.h"

#include "riders/Rider.h"

#include "Request.h"

namespace sim {
    extern EventBus events;
    extern std::vector<Request> requests;
}
