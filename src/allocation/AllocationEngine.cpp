#define DEBUG false

#include "AllocationEngine.h"

#include "Request.h"
#include "riders/Rider.h"
#include "routing/Distance.h"
#include "routing/H3.h"


AllocationEngine::AllocationEngine()
        : riders(sim::riders.size(), 3) {
    sim::events.on<&AllocationEngine::on_request>(*this);
}

void AllocationEngine::on_request(const RequestCreated &event) {
    auto request_id = event.request_id;
    auto &req = sim::requests[request_id];

    rider_id_t best_rider = INVALID_RIDER_ID;
    auto best_pickup_at = std::numeric_limits<timestamp_t>::max();
    auto best_pax = 1000;
    auto last_hex = latlon_to_h3(req.pick_coord);

    auto desired_pax = req.pax > 4 ? 6 : (req.pax > 2 ? 4 : 2);

    for (auto rider_id: riders.candidates(req.pick_coord)) {
        auto &rider = sim::riders[rider_id];

        if (rider.eta_hex() != last_hex) {
            if (best_rider != INVALID_RIDER_ID)
                break;
            else
                last_hex = rider.eta_hex();
        }

        if (rider.n_requests_assigned() >= 2)
            continue;

        auto fm_dist_km = sim::distance(rider.eta_pos(), req.pick_coord);

        // TODO we want to make the allocation engine more modular rather than hard-coding these checks here
        if (!rider_battery.check_capacity(rider_id, fm_dist_km + req.predicted_lm_dist))
            continue;

        auto pax = rider_pax.capacity(rider_id);
        if (pax < req.pax or pax > best_pax)
            continue;

        auto fm_start_at = std::max(rider.eta_at(), sim::clock);
        auto fm_time_s = static_cast<duration_t>(fm_dist_km / speed_kmps);
        auto arrive_pickup_at = fm_start_at + fm_time_s;

        auto pickup_at = arrive_pickup_at + 120;
        if (pickup_at > best_pickup_at and pax == best_pax)
            continue;

        best_rider = rider.id();
        best_pickup_at = pickup_at;
        best_pax = pax;

        if (best_pax == desired_pax and best_pickup_at - sim::clock <= 180)
            break;
    }

    if (best_rider == INVALID_RIDER_ID)
        return;

    auto &rider = sim::riders[best_rider];
    sim::events.trigger(RequestAssigned{request_id, rider.id()});

    rider.push_waypoint({req.pick_coord, 0, req.id, Waypoint::Kind::FirstMile});
    rider.push_waypoint({req.pick_coord, 120, req.id, Waypoint::Kind::WaitForPickup});
    rider.push_waypoint({req.drop_coord, 0, req.id, Waypoint::Kind::LastMile});
    rider.push_waypoint({req.drop_coord, 120, req.id, Waypoint::Kind::WaitForDropoff});

    rider_battery.charge(rider);
}
