#include "AllocationEngine.h"

#include "Order.h"
#include "riders/RiderPool.h"
#include "riders/Rider.h"
#include "routing/RoutingEngine.h"
#include "io/SimulationConfigs.h"
#include "routing/ZoneClusters.h"

namespace sim {
    extern thread_local SimulationConfigs configs;
    extern thread_local ZoneClusters zones;
}

ClusterJIT::ClusterJIT()
        : fm_config("data_sim/fm_config/" + sim::configs.get<std::string>("genesis_name") + ".txt"),
          lm_config("data_sim/lm_config/" + sim::configs.get<std::string>("genesis_name") + ".txt"),
          sla_config("data_sim/sla_config/" + sim::configs.get<std::string>("genesis_name") + ".txt"),
          rph_config("data_sim/rph_config/" + sim::configs.get<std::string>("genesis_name") + ".txt")
          {
}


AllocationResult ClusterJIT::match(const Order &order, const RiderPool &riders) {
    auto [min_lm_km, max_lm_km] = lm_config.get(order.pick_zone, order.customer_id);
    if (max_lm_km < order.predicted_lm_dist || order.predicted_lm_dist < min_lm_km)
        return {};

    AllocationResult result;

    auto [max_kwt_s, sla_deg_s, drop_time_s, speed_kmps] = sla_config.get(order.drop_zone, order.customer_id);
    auto lm_time_s = order.predicted_lm_dist / speed_kmps;

    auto [min_fm_km, max_fm_km] = fm_config.get(order.pick_zone, order.customer_id);
    auto [min_time, max_time] = rph_config.get(order.pick_zone, order.customer_id);
    auto required_pickup_at = order.created_at + order.sla_time + sla_deg_s - drop_time_s - lm_time_s;

    if (order.created_at + order.predicted_ready_time > required_pickup_at)
        return {};

    for (const auto &rider: riders.available_riders()) {
        if (!sim::zones.is_deliverable(rider.zone_id, order.drop_zone))
            continue;

        auto fm_dist_km = sim::distance(rider.eta_pos, order.pick_coord);
        if (max_fm_km <= fm_dist_km || fm_dist_km < min_fm_km)
            continue;

        auto fm_start_at = std::max(rider.eta_at, sim::clock);
        auto fm_time_s = static_cast<duration_t>(fm_dist_km / speed_kmps);
        auto arrive_pickup_at = fm_start_at + fm_time_s;

        if (arrive_pickup_at > required_pickup_at)
            continue;

        auto pickup_at = std::max(arrive_pickup_at, order.created_at + order.predicted_ready_time);
        if (pickup_at - arrive_pickup_at > max_kwt_s)
            continue;

        auto arrive_drop_at = pickup_at + lm_time_s;
        auto completed_at = arrive_drop_at + order.drop_time;
        auto time_on_order = (completed_at - fm_start_at);
        if (time_on_order > max_time or time_on_order < min_time)
            continue;

        result.rider_id = rider.id;
        max_fm_km = fm_dist_km;
    }

    return result;
}

void ClusterJIT::update() {
    fm_config.update();
    lm_config.update();
    sla_config.update();
    rph_config.update();
}
