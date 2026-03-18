#define DEBUG false

#include "AllocationEngine.h"

#include "Request.h"
#include "riders/Rider.h"
#include "riders/RiderBattery.h"
#include "riders/RiderPAX.h"
#include "routing/Distance.h"
#include "routing/H3.h"

// TODO we cannot use global variables these will clash between threads
std::unordered_map<hex_id_t, std::unordered_set<rider_id_t>> hex_id_to_riders;
std::vector<hex_id_t> rider_id_to_hex_id;
RiderBattery rider_battery;
RiderPAX rider_pax;

void AllocationEngine::init() {
    rider_id_to_hex_id.resize(sim::riders.size(), -1);
    rider_battery.init();
    rider_pax.init();
}

void AllocationEngine::on_request(const RequestCreated &event) {
    constexpr speed_t speed_kmps = 40.0 / 3600;

    auto request_id = event.request_id;
    auto &req = sim::requests[request_id];

    rider_id_t best_rider = INVALID_RIDER_ID;
    auto best_pickup_at = std::numeric_limits<timestamp_t>::max();

    auto pick_hex = latlon_to_h3(req.pick_coord);
    debug("AllocationEngine::on_request() _hex_id_to_riders[pick_hex].size() = %zu\n",
          hex_id_to_riders[pick_hex].size());

    for (auto rider_id: hex_id_to_riders[pick_hex]) {
        auto &rider = sim::riders[rider_id];

        if (rider.n_requests_assigned() >= 2)
            continue;

        auto fm_dist_km = sim::distance(rider.eta_pos(), req.pick_coord);

        // TODO we want to make the allocation engine more modular rather than hard-coding these checks here
        if (!rider_battery.check_capacity(rider_id, fm_dist_km + req.predicted_lm_dist))
            continue;

        auto pax = rider_pax.capacity(rider_id);
        if (pax == 1 and req.pax != 1)
            continue;
        if (pax == 2 and req.pax > 2)
            continue;
        if (pax == 4 and req.pax <= 2)
            continue;

        auto fm_start_at = std::max(rider.eta_at(), sim::clock);
        auto fm_time_s = static_cast<duration_t>(fm_dist_km / speed_kmps);
        auto arrive_pickup_at = fm_start_at + fm_time_s;

        auto pickup_at = arrive_pickup_at + 120;
        if (pickup_at > best_pickup_at)
            continue;

        best_rider = rider.id();
        best_pickup_at = pickup_at;

        if (best_pickup_at - sim::clock <= 120)
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

void AllocationEngine::on_rider_updated_eta_pos(const RiderUpdatedETAPos &event) {
    auto rider_id = event.rider_id;
    auto old_hex_id = rider_id_to_hex_id[rider_id];
    auto new_hex_id = sim::riders[rider_id].eta_hex();

    if (old_hex_id == new_hex_id)
        return;

    if (old_hex_id != INVALID_HEX_ID)
        hex_id_to_riders.at(old_hex_id).erase(rider_id);

    hex_id_to_riders[new_hex_id].insert(rider_id);
    rider_id_to_hex_id[rider_id] = new_hex_id;

    #if DEBUG
    size_t n = 0;
    for (auto &[hex_id, rider_ids]: hex_id_to_riders)
        n += rider_ids.size();

    printf("AllocationEngine::on_rider_updated_eta_pos() total riders = %zu\n", n);
    #endif
}
