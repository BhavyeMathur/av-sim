#include "BestPickupStrategy.h"
#include "events/EventBus.h"
#include "riders/RiderManager.h"
#include "routing/grid/Grid.h"


void BoundedH3BestPickupStrategy::assign_request(const Request &req) {
    auto rider = match(req);
    if (!rider)
        return;

    sim::events.trigger(RequestAssigned{req.id, rider->id()});
    sim::riders.push_waypoints(*rider,
                               Waypoint{req.pick_coord, 0, req.id, Waypoint::Kind::FirstMile},
                               Waypoint{req.pick_coord, 120, req.id, Waypoint::Kind::WaitForPickup},
                               Waypoint{req.drop_coord, 0, req.id, Waypoint::Kind::LastMile},
                               Waypoint{req.drop_coord, 120, req.id, Waypoint::Kind::WaitForDropoff});
}

bool BoundedH3BestPickupStrategy::is_better(BoundedH3BestPickupStrategy::RiderInfo &cand,
                                            const BoundedH3BestPickupStrategy::RiderInfo &best) {
    if (cand.pax > best.pax)
        return false;

    auto fm_start_at = std::max(cand.rider->eta_at(), sim::clock);
    auto fm_time_s = static_cast<duration_t>(cand.fm_dist_km / speed_kmps);
    auto arrive_pickup_at = fm_start_at + fm_time_s;
    cand.pickup_at = arrive_pickup_at + 120;

    if (cand.pickup_at > best.pickup_at and cand.pax == best.pax)
        return false;

    return true;
}

Rider *BoundedH3BestPickupStrategy::match(const Request &request) {
    // best rider candidate seen so far
    RiderInfo best;

    // iterate through a pool of riders in order of (pool) priority
    // that is, earlier pools are encountered first and therefore the riders in them
    // are given a higher priority of being matched.
    // by customising the contents of different pools, various strategies can be implemented.
    for (RiderPool &pool: riders.candidate_pools(request)) {
        if (pool.riders.empty())
            continue;

        // pruning based on travel time bound
        if (best.rider != nullptr) {
            // in this strategy, each pool corresponds to a single H3 cell
            // get the H3 cell of this pool from the first rider in it
            // and calculate an approximate lower bound on the travel time
            // skipping this pool if the lower bound leads to a worse pickup time
            auto centroid = grid::cell_to_latlon(pool.cell);
            auto [_, tau] = approx_eta(centroid, request.pick_coord);

            if (best.pickup_at < tau + sim::clock)
                continue;
        }

        // iterate through each rider in a pool, check its feasibility
        // and find the best match using the is_better method which a strategy must provide
        for (auto &rider: pool.riders) {
            RiderInfo info;
            info.rider = &rider;

            if (!is_rider_feasible(rider, request, info))
                continue;

            if (is_better(info, best)) {
                // greedily accept an Idle rider
                if (rider.state() == RiderState::Idle)
                    return &rider;

                best = info;
            }
        }
    }

    return best.rider;
}
