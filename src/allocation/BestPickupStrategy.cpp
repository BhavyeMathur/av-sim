#include "BestPickupStrategy.h"

bool BestPickupStrategy::is_better(BestPickupStrategy::RiderInfo &cand, const BestPickupStrategy::RiderInfo &best) {
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

Strategy::Action GreedyH3BestPickupStrategy::on_pool_end(const HexRidersSource::pool_t &,
                                                         const GreedyH3BestPickupStrategy::RiderInfo &best,
                                                         const Request &) {
    return best.rider ? Strategy::Action::Break : Strategy::Action::None;
}

Strategy::Action BoundedH3BestPickupStrategy::on_pool_start(const HexRidersSource::pool_t &pool,
                                                            const BoundedH3BestPickupStrategy::RiderInfo &best,
                                                            const Request &request) {
    if (pool.empty() or best.rider == nullptr)
        return Strategy::Action::None;

    // in this strategy, each pool corresponds to a single H3 cell
    // get the H3 cell of this pool from the first rider in it
    // and calculate an approximate lower bound on the travel time
    // skipping this pool if the lower bound leads to a worse pickup time
    auto h3_cell = sim::riders[*pool.begin()].eta_hex();
    auto h3_centroid = h3_to_latlon(h3_cell);
    auto [_, tau] = approx_eta(h3_centroid, request.pick_coord);

    if (best.pickup_at < tau + sim::clock)
        return Strategy::Action::Skip;

    return Strategy::Action::None;
}
