#pragma once

#include "includes.h"
#include "events/Event.h"

class AllocationEngine {
public:
    static void on_request(const RequestCreated &event);
};
