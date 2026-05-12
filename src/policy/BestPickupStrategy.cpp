#include "BestPickupStrategy.h"


void BoundedH3BestPickupStrategy::assign_request(const Request &req) {
    auto best = match(req);
    if (!best.rider)
        return;

    auto rider_id = best.rider->id();
    sim::events.trigger(RequestAssigned{req.id, rider_id});

    auto &rider = sim::riders[rider_id];
    rider.push_waypoints(Waypoint{req.pick_coord, 0, req.id, Waypoint::Kind::FirstMile},
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

BoundedH3BestPickupStrategy::RiderInfo BoundedH3BestPickupStrategy::match(const Request &request) {
    // best rider candidate seen so far
    RiderInfo best;

    // iterate through a pool of riders in order of (pool) priority
    // that is, earlier pools are encountered first and therefore the riders in them
    // are given a higher priority of being matched.
    // by customising the contents of different pools, various strategies can be implemented.
    for (auto &pool: riders.candidate_pools(request)) {
        unique_spinlock lock(riders.get_lock(pool.cell));
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
        for (auto rider_id: pool.riders) {
            RiderInfo info;
            info.lck = unique_spinlock::try_acquire(sim::rider_mutexes[rider_id]);
            if (!info.lck.owns_lock())
                continue;

            auto &rider = sim::riders[rider_id];
            info.rider = &rider;

            if (is_rider_feasible(rider, request, info) and is_better(info, best)) {
                best = std::move(info);

                // greedily accept an Idle rider
                if (best.rider->state() == Rider::State::Idle)
                    return best;
            }
        }
    }

    return best;
}
