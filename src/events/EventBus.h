#pragma once

#include "Event.h"
#include <functional>

class EventBus {
public:
    [[nodiscard]] bool empty() const { return events_.empty(); }

    Event pop() { return events_.pop(); }

    mutable_pq<Event>::key push(Event ev) { return events_.push(ev); }

    void trigger(EventPayload payload) { dispatch({last_t_, payload}); }

    void dispatch(const Event &e) {
        assert(ev.t >= last_t_);
        last_t_ = e.t;

        for (const auto &cb: handlers_[e.payload.index()])
            cb(e);
    }

    template<class PayloadT>
        void on(void (*cb)(const PayloadT &)) {
            constexpr std::size_t idx = EventPayload{PayloadT{}}.index();
            handlers_[idx].push_back([cb](const Event &e) {
                return cb(get<PayloadT>(e.payload));
            });
        }

private:
    using callback_t = std::function<void(const Event &)>;
    std::vector<std::vector<callback_t>> handlers_{std::variant_size_v<EventPayload>};

    mutable_pq<Event> events_;

    timestamp_t last_t_ = 0;
};
