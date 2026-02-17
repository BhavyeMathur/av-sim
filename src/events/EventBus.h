#pragma once

#include "Event.h"
#include "util/function.h"


// EventBus wraps a queue of Events and allows the user to
// register and run custom callback functions for events
class EventBus {
public:
    [[nodiscard]] bool empty() const { return events_.empty(); }

    Event pop() { return events_.pop(); }

    mutable_pq<Event>::key push(Event ev) { return events_.push(ev); }

    void trigger(EventPayload payload) { dispatch({last_t_, payload}); }

    void dispatch(const Event &e) {
        // ensure monotonicity of event times
        assert(e.t >= last_t_);
        last_t_ = e.t;

        // the event payload index corresponds to the type of the event (inferred from the variant)
        // the callbacks for this event type are defined in a vector of callbacks, handler_
        // the callback expects a raw void * to the callback function + the event itself
        for (const auto &h: handlers_[e.payload.index()])
            h.call(h.fn, e);
    }

    template<class PayloadT>
        void on(void (*callback)(const PayloadT &)) {
            // there is some additional complexity in this templated function so that we are able to
            // automatically infer the kind of the event
            // (i.e. the user does not have to specify PayloadT since this can be inferred from the callback)
            // we do not support capturing lambdas (non-capturing lambdas and ordinary functions are supported)

            constexpr std::size_t idx = EventPayload{PayloadT{}}.index();
            handlers_[idx].push_back({reinterpret_cast<void *>(callback),

                                      [](void *fn, const Event &e) {
                                          auto cb = reinterpret_cast<void (*)(const PayloadT &)>(fn);
                                          cb(std::get<PayloadT>(e.payload));
                                      }
                                     });
        }

    template<class F>
        void on(F cb) {
            using PayloadT = first_arg_t<F>;
            on<PayloadT>(+cb);  // +cb converts non-capturing lambda to function pointer
        }

private:
    struct Handler {
        void *fn;

        void (*call)(void *, const Event &);
    };

    std::vector<std::vector<Handler>> handlers_{std::variant_size_v<EventPayload>};

    mutable_pq<Event> events_;

    timestamp_t last_t_ = 0;
};
