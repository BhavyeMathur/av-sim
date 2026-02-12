#include "RoutingEngine.h"

#include "Order.h"
#include "riders/Rider.h"
#include "allocation/AllocationEngine.h"
#include "io/Statistics.h"
#include "routing/EVCharging.h"


namespace sim {
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

    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, Coordinate p2) {
        return distance(lat1_rad, lon1_rad, p2.lat, p2.lon);
    }

    distance_t distance(Coordinate p1, coordinate_t lat2_rad, coordinate_t lon2_rad) {
        return distance(p1.lat, p1.lon, lat2_rad, lon2_rad);;
    }

    distance_t distance(Coordinate p1, Coordinate p2) {
        return distance(p1.lat, p1.lon, p2.lat, p2.lon);
    }
}

RoutingEngine::RoutingEngine(const std::string &speed_config_file)
        : m_speed(speed_config_file) {
}

void RoutingEngine::update() {
    m_speed.update();
}

AllocationStatistic RoutingEngine::assign_order(const AllocationResult &allocation,
                                                Rider &rider, const Order &order) const {
    auto fm_dist_km = sim::distance(rider.eta_pos, order.pick_coord);
    auto lm_dist_km = sim::distance(order.pick_coord, order.drop_coord);

    auto fm_time_s = static_cast<duration_t>(fm_dist_km / m_speed.fm_speed_kmps());
    auto lm_time_s = static_cast<duration_t>(lm_dist_km / m_speed.lm_speed_kmps());

    auto fm_start_at = std::max(rider.eta_at, sim::clock);
    auto arrive_pickup_at = fm_start_at + fm_time_s;
    auto pickup_at = std::max(arrive_pickup_at, order.created_at + order.ready_time) + order.pick_time;
    auto arrive_drop_at = pickup_at + lm_time_s;
    auto finish_at = arrive_drop_at + order.drop_time;

    rider.add_assignment(order, finish_at);

    rider.add_waypoint({order.id, fm_time_s, RiderWaypoint::Type::GOTO_PICKUP});
    rider.add_waypoint({order.id, pickup_at - arrive_pickup_at, RiderWaypoint::Type::PICKUP});
    rider.add_waypoint({order.id, lm_time_s, RiderWaypoint::Type::GOTO_DROP});
    rider.add_waypoint({order.id, order.drop_time, RiderWaypoint::Type::DROP});

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
            .drop_time = order.drop_time,
    };
}

void RoutingEngine::weak_assign(Rider &rider, Coordinate to) const {
    auto distance = sim::distance(rider.eta_pos, to);
    auto time = static_cast<duration_t>(distance / m_speed.ambient_speed_kmps());
    rider.weak_assign(to, time);
}
