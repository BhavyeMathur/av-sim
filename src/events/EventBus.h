#pragma once

#include "Event.h"
#include "util/function.h"
#include <queues.h>


// EventBus wraps a queue of Events and allows the user to
// register and run custom callback functions for events
class EventBus {
public:
    static bool empty() { return events_.empty(); }

    [[nodiscard]] static size_t size() { return events_.size(); }

    static Event pop() { return events_.pop(); }

    static mutable_pq<Event>::key push(Event ev) { return events_.push(ev); }

    void trigger(EventPayload payload) { dispatch({last_t_, payload}); }

    void dispatch(const Event &e) {
        // ensure monotonicity of event times
        // assert(e.t >= last_t_);
        last_t_ = e.t;
        auto idx = e.payload.index();

        // the event payload index corresponds to the type of the event (inferred from the variant)
        // the callbacks for this event type are defined in a vector of callbacks, handler_
        // the callback expects a raw void * to the callback function + the event itself
        for (const auto &h: handlers_[idx])
            h.invoke(h.ctx, e);
    }

    // free functions/non-capturing lambdas
    template<class PayloadT>
        void on(void (*callback)(const PayloadT &)) {
            // there is some additional complexity in this templated function so that we are able to
            // automatically infer the kind of the event
            // (i.e. the user does not have to specify PayloadT since this can be inferred from the callback)
            // we do not support capturing lambdas (non-capturing lambdas and ordinary functions are supported)
            constexpr std::size_t idx = EventPayload{PayloadT{}}.index();

            handlers_[idx].push_back({reinterpret_cast<void *>(callback),
                                      [](void *fn, const Event &e) noexcept {
                                          auto cb = reinterpret_cast<void (*)(const PayloadT &)>(fn);
                                          cb(std::get<PayloadT>(e.payload));
                                      }
                                     });
        }

    template<class F>
        requires FunctionPointerLike<F>
        void on(F &&cb) {
            using PayloadT = first_arg_fn_t<decltype(+cb)>;
            on<PayloadT>(+cb);
        }

    // capturing lambdas/functors by lvalue
    template<class PayloadT, class F>
        requires (!FunctionPointerLike<F>)
        void on(F &cb) {
            constexpr std::size_t idx = EventPayload{PayloadT{}}.index();

            handlers_[idx].push_back({static_cast<void *>(&cb),
                                      [](void *p, const Event &e) noexcept {
                                          auto &fn = *static_cast<F *>(p);
                                          fn(std::get<PayloadT>(e.payload));
                                      }
                                     });
        }

    template<class F>
        requires (!FunctionPointerLike<F>)
        void on(F &cb) {
            using PayloadT = first_arg_functor_t<F>;
            on<PayloadT>(cb);
        }

    // member methods
    template<auto Method, class C>
        requires std::is_base_of_v<
                typename method_traits<decltype(Method)>::class_type,
                std::remove_reference_t<C>
        >
        void on(C &obj) {
            using traits = method_traits<decltype(Method)>;
            using PayloadT = typename traits::arg_type;

            constexpr std::size_t idx = EventPayload{PayloadT{}}.index();

            handlers_[idx].push_back({static_cast<void *>(&obj),
                                      [](void *p, const Event &e) noexcept {
                                          auto &self = *static_cast<C *>(p);
                                          (self.*Method)(std::get<PayloadT>(e.payload));
                                      }
                                     });
        }

private:
    struct Handler {
        void *ctx = nullptr;

        void (*invoke)(void *, const Event &) noexcept = nullptr;
    };

    std::array<std::vector<Handler>, std::variant_size_v<EventPayload>> handlers_;

    struct _event_radix_key {
        using key_type = timestamp_t;

        key_type operator()(const Event &value) const {
            return value.t;
        }
    };

    static mutable_pq<Event> events_;
    // static mutable_radix_heap<Event, _event_radix_key> events_;

    timestamp_t last_t_ = 0;
};
