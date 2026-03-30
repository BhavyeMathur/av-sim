#include "Strategy.h"

Strategy::Strategy() {
    sim::events.on<&Strategy::on_request>(*this);
}

void Strategy::on_request(const RequestCreated &event) {
    auto request_id = event.request_id;
    auto &req = sim::requests[request_id];

    auto rider_id = match(req);
    if (rider_id == INVALID_RIDER_ID)
        return;

    auto &rider = sim::riders[rider_id];
    sim::events.trigger(RequestAssigned{request_id, rider.id()});

    rider.push_waypoint({req.pick_coord, 0, req.id, Waypoint::Kind::FirstMile});
    rider.push_waypoint({req.pick_coord, 120, req.id, Waypoint::Kind::WaitForPickup});
    rider.push_waypoint({req.drop_coord, 0, req.id, Waypoint::Kind::LastMile});
    rider.push_waypoint({req.drop_coord, 120, req.id, Waypoint::Kind::WaitForDropoff});

    rider_battery.charge(rider);
}

bool Strategy::is_rider_feasible(const Rider &rider, const Request &request, Strategy::RiderInfo &info) {
    if (rider.n_requests_assigned() >= 2)
        return false;

    info.fm_dist_km = sim::distance(rider.eta_pos(), request.pick_coord);
    if (!rider_battery.check_capacity(rider.id(), info.fm_dist_km + request.predicted_lm_dist))
        return false;

    info.pax = rider_pax.capacity(rider.id());
    if (info.pax < request.pax)
        return false;

    return true;
}
