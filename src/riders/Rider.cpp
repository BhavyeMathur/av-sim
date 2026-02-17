#include "Rider.h"

rider_id_t  Rider::next_id_ = 0;

void Rider::on_waypoint(const Event &event) {
    auto rider_id = get<RiderWaypoint>(event.payload).rider_id;
    sim::riders[rider_id].complete_waypoint_(event.t);
}

void Rider::on_login(const Event &event) {
    auto rider_id = get<RiderLogin>(event.payload).rider_id;
    sim::riders[rider_id].login(event.t);
}

void Rider::on_logout(const Event &event) {
    auto rider_id = get<RiderLogout>(event.payload).rider_id;
    sim::riders[rider_id].logoff();
}
