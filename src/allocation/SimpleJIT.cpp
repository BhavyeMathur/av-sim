#include "AllocationEngine.h"

#include "Order.h"
#include "riders/RiderPool.h"
#include "riders/Rider.h"
#include "routing/RoutingEngine.h"
#include "io/SimulationConfigs.h"

namespace sim {
    extern thread_local SimulationConfigs configs;
    extern thread_local timestamp_t clock;
}

SimpleJIT::SimpleJIT()
        : fm_cutoff_km(sim::configs.get<distance_t>("fm_cutoff_km")) {
}

AllocationResult SimpleJIT::match(const Order &order, const RiderPool &riders) {
    AllocationResult result;
    auto best_pickup_at = std::numeric_limits<timestamp_t>::max();

    for (const auto &rider: riders.available_riders()) {
        auto fm_dist_km = sim::distance(rider.eta_pos, order.pick_coord);
        if (fm_dist_km > fm_cutoff_km)
            continue;

        #if SIM_FEATURE_EV_CHARGING
        if (fm_dist_km + order.predicted_lm_dist + 20 > rider.range_km)
            continue;
        #endif

        auto fm_start_at = std::max(rider.eta_at, sim::clock);
        auto fm_time_s = static_cast<duration_t>(fm_dist_km / speed_kmps);
        auto arrive_pickup_at = fm_start_at + fm_time_s;

        auto pickup_at = std::max(arrive_pickup_at, order.created_at + order.predicted_ready_time) + order.pick_time;
        if (pickup_at > best_pickup_at)
            continue;

        result.rider_id = rider.id;
        best_pickup_at = pickup_at;
    }

    return result;
}
