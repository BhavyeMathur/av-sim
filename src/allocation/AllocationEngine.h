#pragma once

#include "includes.h"
#include "events/Event.h"

class AllocationEngine {
public:
    AllocationEngine();

    void on_request(const RequestCreated &event);

private:
    HexRidersSource riders;
    RiderBattery rider_battery;
    RiderPAX rider_pax;

    static constexpr speed_t speed_kmps = 40.0 / 3600;
};
