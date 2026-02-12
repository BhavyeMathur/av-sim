#include "RoutingEngine.h"

#include "Order.h"
#include "riders/Rider.h"
#include "allocation/AllocationEngine.h"
#include "io/Statistics.h"
#include "routing/EVCharging.h"


namespace sim {
    extern thread_local timestamp_t clock;
    #if SIM_FEATURE_EV_CHARGING
    extern thread_local EVCharging ev_charging;
    #endif
}

namespace sim {
    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad,
                        coordinate_t lat2_rad, coordinate_t lon2_rad) {
        return static_cast<distance_t>(0.572)
               + static_cast<distance_t>(1.273) * equirectangular_distance(lat1_rad, lon1_rad, lat2_rad, lon2_rad);
    }

    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, coordinate p2) {
        return distance(lat1_rad, lon1_rad, p2.lat, p2.lon);
    }

    distance_t distance(coordinate p1, coordinate_t lat2_rad, coordinate_t lon2_rad) {
        return distance(p1.lat, p1.lon, lat2_rad, lon2_rad);;
    }

    distance_t distance(coordinate p1, coordinate p2) {
        return distance(p1.lat, p1.lon, p2.lat, p2.lon);
    }
}

AllocationStatistic RoutingEngine::assign_order(const AllocationResult &allocation,
                                                Rider &rider, const Order &order) const {
    auto fm_dist_km = sim::distance(rider.eta_pos, order.pick_coord);
    auto lm_dist_km = sim::distance(order.pick_coord, order.drop_coord);

    auto fm_time_s = static_cast<duration_t>(fm_dist_km / m_speed);
    auto lm_time_s = static_cast<duration_t>(lm_dist_km / m_speed);

    auto fm_start_at = std::max(rider.eta_at, sim::clock);
    auto arrive_pickup_at = fm_start_at + fm_time_s;
    auto pickup_at = arrive_pickup_at + pick_time_s;
    auto arrive_drop_at = pickup_at + lm_time_s;
    auto finish_at = arrive_drop_at + drop_time_s;

    rider.add_assignment(order, finish_at);

    rider.add_waypoint({order.id, fm_time_s, RiderWaypoint::Type::GOTO_PICKUP});
    rider.add_waypoint({order.id, pickup_at - arrive_pickup_at, RiderWaypoint::Type::PICKUP});
    rider.add_waypoint({order.id, lm_time_s, RiderWaypoint::Type::GOTO_DROP});
    rider.add_waypoint({order.id, drop_time_s, RiderWaypoint::Type::DROP});

    #if SIM_FEATURE_EV_CHARGING
    sim::ev_charging.update(rider, fm_dist_km + lm_dist_km);
    #endif

    return {
            .rider_id = *allocation.rider_id,
            .order_id = order.id,

            .fm_dist = fm_dist_km,
            .lm_dist = lm_dist_km,

            .start_at = fm_start_at,
            .pickup_at = pickup_at,

            .fm_time = fm_time_s,
            .wait_time = pickup_at - arrive_pickup_at,
            .lm_time = lm_time_s,
            .drop_time = drop_time_s,
    };
}

void RoutingEngine::weak_assign(Rider &rider, coordinate to) const {
    auto distance = sim::distance(rider.eta_pos, to);
    auto time = static_cast<duration_t>(distance / m_speed);
    rider.weak_assign(to, time);
}
