#pragma once

#include "includes.h"
#include "EventBus.h"

namespace sim {
    extern thread_local EventBus events;
}

template<class PayloadT>
    class EventLock {
    public:
        EventLock() { sim::events.disable<PayloadT>(); }

        ~EventLock() { sim::events.enable<PayloadT>(); }

    };
