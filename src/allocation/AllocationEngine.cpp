#include "AllocationEngine.h"

#include "Request.h"
#include "riders/Rider.h"
#include "routing/Distance.h"


void AllocationEngine::on_request(const RequestCreated &event) {
    auto request_id = event.request_id;
    auto &request = sim::requests[request_id];

    rider_id_t best_rider = -1;
    distance_t best_dist = std::numeric_limits<distance_t>::max();

    for (auto &rider: sim::riders) {
        if (!rider.is_idle())
            continue;

        auto dist = sim::distance(request.pick_coord, rider.eta_pos());
        if (dist < best_dist) {
            best_rider = rider.id();
            best_dist = dist;

            if (dist < 2) // km
                break;
        }
    }

    if (best_rider == -1) {
        // printf("dropping request %i\n", request.id);
        return;
    }

    // printf("assigning request %i to rider %i\n", request.id, best_rider);

    auto &rider = sim::riders[best_rider];
    sim::events.trigger(RequestAssigned{request_id, rider.id()});

    rider.push_waypoint({request.pick_coord, 0, request.id, Waypoint::Kind::FirstMile});
    rider.push_waypoint({request.pick_coord, 120, request.id, Waypoint::Kind::WaitForPickup});
    rider.push_waypoint({request.drop_coord, 0, request.id, Waypoint::Kind::LastMile});
    rider.push_waypoint({request.drop_coord, 120, request.id, Waypoint::Kind::WaitForDropoff});
}
