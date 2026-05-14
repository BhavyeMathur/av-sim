#pragma once

#include "includes.h"
#include "EventBus.h"

template<class PayloadT>
    class EventLock {
    public:
        EventLock() { sim::events.disable<PayloadT>(); }

        ~EventLock() { sim::events.enable<PayloadT>(); }
    };
