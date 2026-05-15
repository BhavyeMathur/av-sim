#include "VehicleBatching.h"
#include "events/EventBus.h"
#include "riders/RiderManager.h"
#include "routing/grid/Grid.h"
#include "io/SimulationConfigs.h"


void VehicleBatching::assign_request(const Request &req) {
    constexpr uint8_t MAX_BATCH = 3;
    const uint8_t need = (req.pax + 1) / 2;

    std::array<RiderInfo, MAX_BATCH> best;
    uint8_t nbest = 0;

    auto worst_vehicle = [&]() {
        return std::max_element(best.begin(), best.begin() + nbest,
                                [](const RiderInfo &a, const RiderInfo &b) {
                                    return a.pickup_at < b.pickup_at;
                                });
    };

    for (RiderPool &pool: candidate_pools(req)) {
        if (pool.riders.empty())
            continue;

        // bounded H3 strategy
        if (nbest >= need) {
            auto centroid = grid::cell_to_latlon(pool.cell);
            auto [_, tau] = approx_eta(centroid, req.pick_coord);

            if (worst_vehicle()->pickup_at < tau + sim::clock)
                continue;
        }
        // bounded H3 strategy

        for (auto &rider: pool.riders) {

            RiderInfo cand;
            cand.rider = &rider;

            if (!is_rider_feasible<false>(rider, req, cand))
                continue;

            auto fm_start_at = std::max(rider.eta_at(), sim::clock);
            auto fm_time_s = static_cast<duration_t>(cand.fm_dist_km / speed_kmps);
            auto arrive_pickup_at = fm_start_at + fm_time_s;

            if (arrive_pickup_at >= sim::clock + sim::configs.policy.max_response_time)
                continue;

            cand.pickup_at = arrive_pickup_at + 120;
            if (nbest < need)
                best[nbest++] = cand;
            else {
                auto worst = worst_vehicle();
                if (cand.pickup_at < worst->pickup_at)
                    *worst = cand;
            }

            // Greedy accept only once the whole batch is full.
            if (nbest == need && worst_vehicle()->rider->state() == RiderState::Idle)
                goto ret;
        }
    }

    if (nbest < need)
        return;
    assert(nbest == need);

    ret:
    auto slowest = worst_vehicle();
    auto pickup_at = slowest->pickup_at;

    #pragma unroll
    for (uint8_t i = 0; i < need; i++) {
        auto &rider_info = best[i];
        auto *rider = rider_info.rider;

        auto dwell_s = pickup_at - rider_info.pickup_at;

        sim::events.trigger(RequestAssigned{req.id, rider->id()});
        sim::riders.push_waypoints(*rider,
                                   Waypoint{req.pick_coord, 0, req.id, Waypoint::Kind::FirstMile},
                                   Waypoint{req.pick_coord, 120 + dwell_s, req.id, Waypoint::Kind::WaitForPickup},
                                   Waypoint{req.drop_coord, 0, req.id, Waypoint::Kind::LastMile},
                                   Waypoint{req.drop_coord, 120, req.id, Waypoint::Kind::WaitForDropoff});
    }
}
