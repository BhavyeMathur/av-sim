#include "Rider.h"
#include "Order.h"

#include "io/Statistics.h"
#include "routing/EVCharging.h"


using namespace std;

namespace sim {
    extern thread_local timestamp_t clock;
    extern thread_local Statistics stats;

    #if SIM_FEATURE_EV_CHARGING
    extern thread_local EVCharging ev_charging;
    #endif
}

void Rider::add_waypoint(RiderWaypoint waypoint) {
    if (waypoints.empty()) {
        last_waypoint_at = sim::clock;

        changed = true;
        switch (waypoint.type) {
            case RiderWaypoint::Type::GOTO_PICKUP:
                state = State::FM;
                break;

            case RiderWaypoint::Type::WANDER:
                state = State::WANDER;
                break;

            default:
                throw std::invalid_argument("Rider::add_waypoint() assert failed: invalid waypoint type");
        }
    }

    waypoints.push_back(waypoint);
}

void Rider::add_assignment(const Order &order, timestamp_t finish_at) {
    if (state == State::WANDER)
        _complete_waypoint();

    eta_pos = order.drop_coord;
    eta_at = finish_at;
    n_active_orders++;
}

void Rider::weak_assign(coordinate to, timestamp_t duration) {
    assert(state == State::IDLE);

    velocity = (to - eta_pos) / static_cast<coordinate_t>(duration);
    add_waypoint({0, duration, RiderWaypoint::Type::WANDER});
}

bool Rider::update() {
    _assert_state();
    assert(last_waypoint_at <= sim::clock);

    while (!waypoints.empty() and waypoints.front().duration <= sim::clock - last_waypoint_at) {
        _complete_waypoint();
        if (waypoints.empty())
            break;

        switch (waypoints.front().type) {
            case RiderWaypoint::Type::GOTO_PICKUP:
                state = State::FM;
                break;

            case RiderWaypoint::Type::PICKUP:
                state = State::WAIT;
                break;

            case RiderWaypoint::Type::GOTO_DROP:
                state = State::LM;
                break;

            case RiderWaypoint::Type::DROP:
                state = State::DROP;
                break;

            case RiderWaypoint::Type::SERVICE:
                state = State::UNAVAILABLE;

            default:
                break;
        }
    }

    if (state == State::WANDER)
        eta_pos += velocity;

    if (changed) {
        changed = false;
        return true;
    }
    return false;
}

void Rider::spawn() {
    assert(!is_alive());
    state = Rider::State::IDLE;

    #if SIM_FEATURE_EV_CHARGING
    range_km = sim::ev_charging.start_with_charge();
    #endif
}

void Rider::kill() {
    assert(is_alive());
    state = State::DEAD;
}

bool Rider::is_alive() const {
    return state != State::DEAD;
}

bool Rider::is_free() const {
    return n_active_orders == 0;
}

RiderWaypoint Rider::_complete_waypoint() {
    assert(!waypoints.empty());
    changed = true;

    auto waypoint = waypoints.front();
    last_waypoint_at += waypoint.duration;

    switch (waypoint.type) {
        case RiderWaypoint::Type::DROP:
            n_active_orders--;
            break;

        default:
            break;
    }

    waypoints.pop_front();
    if (waypoints.empty())
        state = State::IDLE;

    return waypoint;
}

void Rider::_assert_state() const {
    assert(n_active_orders <= 2);

    if (is_free())
        assert(state == State::IDLE or state == State::WANDER);
    else
        assert(state == State::FM or state == State::WAIT or state == State::LM or state == State::DROP);

    switch (state) {
        case State::IDLE:
            assert(waypoints.empty());
            break;

        case State::WANDER:
            assert(waypoints.size() == 1);
            break;

        default:
            break;
    }
}
