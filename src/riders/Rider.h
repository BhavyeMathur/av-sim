#pragma once

#include "includes.h"
#include <ringbuffer.h>

struct Order;

struct RiderWaypoint {
    order_id_t order_id;
    duration_t duration;

    enum Type : uint8_t {
        WANDER, GOTO_PICKUP, PICKUP, GOTO_DROP, DROP, SERVICE,
    } type;
};


// TODO encapsulate rider
struct Rider {
    ringbuffer<RiderWaypoint> waypoints;
    timestamp_t last_waypoint_at = 0;

    rider_id_t id;
    timestamp_t logout_at;

    timestamp_t eta_at = 0;
    Coordinate eta_pos;
    Coordinate velocity{};

    #if SIM_FEATURE_EV_CHARGING
    distance_t range_km = 0;
    #endif

    zone_id_t zone_id;

    uint8_t n_active_orders = 0;
    bool changed = false;

    enum State : uint8_t {
        DEAD, IDLE, FM, WAIT, LM, DROP, WANDER, UNAVAILABLE
    } state = DEAD;

    Rider(rider_id_t id, Coordinate pos, timestamp_t login_at, timestamp_t logout_at, zone_id_t zone_id)
            : id(id), logout_at(logout_at), eta_at(login_at), eta_pos(pos), zone_id(zone_id) {
    }

    void add_waypoint(RiderWaypoint waypoint);

    void add_assignment(const Order &order, timestamp_t finish_at);

    void weak_assign(Coordinate to, timestamp_t duration);

    bool update();

    void spawn();

    void kill();

    [[nodiscard]] bool is_alive() const;

    [[nodiscard]] bool is_free() const;

private:
    RiderWaypoint _complete_waypoint();

    void _assert_state() const;
};
