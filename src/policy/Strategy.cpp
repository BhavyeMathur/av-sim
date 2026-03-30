#include "Strategy.h"
#include "extern.h"


Strategy::Strategy() {
    sim::events.on<&Strategy::on_request>(*this);
}

void Strategy::on_request(const RequestCreated &event) {
    auto &req = sim::requests[event.request_id];

    auto rider_id = match(req);
    if (rider_id == INVALID_RIDER_ID)
        return;

    auto &rider = sim::riders[rider_id];
    sim::events.trigger(RequestAssigned{req.id, rider.id()});

    rider.push_waypoints(Waypoint{req.pick_coord, 0, req.id, Waypoint::Kind::FirstMile},
                         Waypoint{req.pick_coord, 120, req.id, Waypoint::Kind::WaitForPickup},
                         Waypoint{req.drop_coord, 0, req.id, Waypoint::Kind::LastMile},
                         Waypoint{req.drop_coord, 120, req.id, Waypoint::Kind::WaitForDropoff});
}

bool Strategy::is_rider_feasible(const Rider &rider, const Request &request, Strategy::RiderInfo &info) {
    if (rider.n_requests_assigned() >= 2)
        return false;

    info.fm_dist_km = sim::distance(rider.eta_pos(), request.pick_coord);
    if (!sim::rider_battery.check_capacity(rider.id(), info.fm_dist_km + request.predicted_lm_dist))
        return false;

    info.pax = sim::rider_pax.capacity(rider.id());
    if (info.pax < request.pax)
        return false;

    return true;
}
