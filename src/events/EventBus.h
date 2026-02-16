#pragma once

#include "Event.h"

class EventBus {
public:
    using callback_t = void (*)(const Event &);

    EventBus() : handlers_((size_t) EventType::COUNT) {}

    void on(EventType t, callback_t cb) {
        handlers_[(size_t) t].push_back(cb);
    }

    void dispatch(const Event &ev) const {
        const auto &hs = handlers_[(size_t) ev.type];
        for (auto callback: hs)
            callback(ev);
    }

private:
    std::vector<std::vector<callback_t>> handlers_;
};
