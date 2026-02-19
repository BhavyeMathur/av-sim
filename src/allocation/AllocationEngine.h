#pragma once

#include "includes.h"
#include "events/Event.h"

class AllocationEngine {
public:
    static void init();

    static void on_request(const RequestCreated &event);

    static void on_rider_updated_eta_pos(const RiderUpdatedETAPos &event);
};
