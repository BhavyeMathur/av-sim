#pragma once

#include "includes.h"

#include "Request.h"
#include "riders/RiderBattery.h"
#include "riders/RiderPAX.h"
#include "riders/RiderSource.h"
#include "routing/Distance.h"
#include "events/EventBus.h"

namespace sim {
    extern EventBus events;

    extern RiderBattery rider_battery;
    extern RiderPAX rider_pax;
}

class Strategy {
public:
    // the base strategy contains a struct "RiderInfo" which is computed every time is_rider_feasible is called
    // the struct may be partially initialized when is_rider_feasible returns false.
    struct RiderInfo {
        Rider *rider = nullptr;
        uint8_t pax = std::numeric_limits<uint8_t>::max();
        distance_t fm_dist_km = std::numeric_limits<distance_t>::max();
    };

    // user-implemented strategies communicate with the backend by returning actions
    // that indicate how the control flow in the backend strategy should proceed
    enum class Action {
        None,
        Skip,
        Break,
    };

    Strategy();

    virtual ~Strategy() = default;

protected:
    // checks for global feasibility parameters such as
    //  1. passenger capacity (pax)
    //  2. battery life
    template<bool check_pax = true>
        [[nodiscard]] static bool is_rider_feasible(const Rider &rider, const Request &request, RiderInfo &info) {
            if (rider.n_requests_assigned() >= 2)
                return false;

            info.fm_dist_km = sim::distance(rider.eta_pos(), request.pick_coord);
            if (!sim::rider_battery.check_capacity(rider.id(), info.fm_dist_km + request.predicted_lm_dist))
                return false;

            if constexpr (check_pax) {
                info.pax = sim::rider_pax.capacity(rider.id());
                if (info.pax < request.pax)
                    return false;
            }

            return true;
        }

private:
    virtual void assign_request(const Request &request) = 0;

    void on_request(const RequestCreated &event);
};

template<class Derived>
    class SequentialStrategy : public Strategy {
    private:
        // checks if Pool is an iterable over rider_id_t
        template<typename Pool>
            static constexpr bool rider_pool_v =
                    std::ranges::input_range<Pool> and
                    std::convertible_to<std::ranges::range_value_t<Pool>, rider_id_t>;

        // checks if Pools is an iterable over an iterable of rider_id_t
        template<typename Pools>
            static constexpr bool rider_candidate_pools_range_v =
                    std::ranges::input_range<Pools> and
                    rider_pool_v<std::ranges::range_reference_t<Pools>>;

        // checks if Derived implements all the required methods
        // needed to implement a sequential strategy
        static constexpr bool sequential_strategy_v =
                std::derived_from<Derived, Strategy> and
                requires(Derived object, const Derived cobject, const Request &req) {
                    // every strategy contains a RiderInfo struct (the default is provided by BaseStrategy)
                    // this must be a subclass of BaseStrategy::RiderInfo so that the backend architecture has access
                    // to all the data it needs
                    typename Derived::RiderInfo;
                    requires std::derived_from<typename Derived::RiderInfo, Strategy::RiderInfo>;

                    // a sequential strategy is defined by a candidate_pools() and is_better() method which
                    // allows us to iterate through a pool of rider candidates for a given request
                    // and compare riders (pairwise) to select the best one
                    { cobject.candidate_pools(req) };
                    requires rider_candidate_pools_range_v<decltype(cobject.candidate_pools(req))>;

                    // is_better takes in the candidate RiderInfo (as non-const so that it can initialise its elements)
                    // and a const reference to the current best candidate, compares them and returns a boolean
                    {
                    Derived::is_better(
                            std::declval<typename Derived::RiderInfo &>(),
                            std::as_const(std::declval<typename Derived::RiderInfo &>())
                    )
                    } -> std::same_as<bool>;
                };

        void assign_request(const Request &req) override {
            auto rider_id = match(req);
            if (rider_id == INVALID_RIDER_ID)
                return;

            auto &rider = sim::riders[rider_id];
            sim::events.trigger(RequestAssigned{req.id, rider.id()});

            rider.push_waypoints(Waypoint{req.pick_coord, 0, req.id, Waypoint::Kind::FirstMile},
                                 Waypoint{req.pick_coord, 120, req.id, Waypoint::Kind::WaitForPickup},
                                 Waypoint{req.drop_coord, 0, req.id, Waypoint::Kind::LastMile},
                                 Waypoint{req.drop_coord, 120, req.id, Waypoint::Kind::WaitForDropoff});
        }

        rider_id_t match(const Request &request) {
            static_assert(sequential_strategy_v);

            // this is the data type of the iterable containing pools of candidate riders
            // for example, a sequential strategy might return a vector of sets of riders
            using candidate_pools_t = decltype(std::declval<const Derived &>().candidate_pools(
                    std::declval<const Request &>()));

            // this is the data type of the pool of candidate riders (commonly, vector/set/array of rider_id_t)
            using pool_t = std::remove_cvref_t<std::ranges::range_reference_t<candidate_pools_t>>;

            // these expressions check if the class provides on_pool_start and on_pool_end methods
            // that can be called inside the outer iteration loop of the match function
            // this is done by attempting to static_cast the methods in the derived class to the
            // correct function type. if the cast succeeds, the function must exist
            static constexpr bool has_on_pool_start_v = requires {
                static_cast<
                        Strategy::Action (Derived::*)(const pool_t &,
                                                      const typename Derived::RiderInfo &,
                                                      const Request &)
                        >(&Derived::on_pool_start);
            };

            static constexpr bool has_on_pool_end_v = requires {
                static_cast<
                        Strategy::Action (Derived::*)(const pool_t &,
                                                      const typename Derived::RiderInfo &,
                                                      const Request &)
                        >(&Derived::on_pool_end);
            };

            // best rider candidate seen so far
            typename Derived::RiderInfo best;

            // iterate through a pool of riders in order of (pool) priority
            // that is, earlier pools are encountered first and therefore the riders in them
            // are given a higher priority of being matched.
            // by customising the contents of different pools, various strategies can be implemented.
            for (auto &pool: static_cast<Derived *>(this)->candidate_pools(request)) {
                // if the strategy provides a on_pool_start function, run it and perform an action
                if constexpr (has_on_pool_start_v) {
                    auto action = static_cast<Derived *>(this)->on_pool_start(pool, best, request);
                    if (action == Action::Break) break;
                    if (action == Action::Skip) continue;
                }

                // iterate through each rider in a pool, check its feasibility
                // and find the best match using the is_better method which a strategy must provide
                for (auto rider_id: pool) {
                    auto &rider = sim::riders[rider_id];

                    typename Derived::RiderInfo info;
                    info.rider = &rider;

                    if (!is_rider_feasible(rider, request, info))
                        continue;

                    if (static_cast<Derived *>(this)->is_better(info, best))
                        best = info;
                }

                // if the strategy provides a on_pool_end function, run it and perform an action
                if constexpr (has_on_pool_end_v) {
                    auto action = static_cast<Derived *>(this)->on_pool_end(pool, best, request);
                    if (action == Action::Break) break;
                    if (action == Action::Skip) continue;
                }
            }

            return best.rider ? best.rider->id() : INVALID_RIDER_ID;
        }
    };
