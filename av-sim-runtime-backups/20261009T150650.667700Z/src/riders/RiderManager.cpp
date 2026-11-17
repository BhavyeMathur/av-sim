#include "RiderManager.h"
#include "Rider.h"
#include "events/EventLock.h"
#include "io/RidersDataframe.h"
#include "io/SimulationConfigs.h"

#include <random>

RiderManager::RiderManager(const RidersDataFrame &df) {
    std::mt19937 rng(std::random_device{}());
    std::discrete_distribution<int> dist{
            sim::configs.fleet.frac_2_seater,
            sim::configs.fleet.frac_4_seater,
            sim::configs.fleet.frac_6_seater
    };

    rider_id_t id = 0;
    for (const auto &r: df) {
        uint8_t pax;
        switch (dist(rng)) {
            case 0:
                pax = 2;
                break;
            case 1:
                pax = 4;
                break;
            case 2:
                pax = 6;
                break;
        }

        coordinate pos(static_cast<coordinate_t>(r.lat), static_cast<coordinate_t>(r.lon));

        emplace(id, pos, pax);
        rider_id_to_data_[id].pos_ = pos;
        id++;
    }
}

RiderData &RiderManager::get_data(rider_id_t rider_id) {
    return rider_id_to_data_[rider_id];
}

void RiderManager::charge(rider_id_t rider_id, coordinate at) {
    auto &rider = get_rider(rider_id);
    return charge(rider, at);
}

void RiderManager::charge(Rider &rider, coordinate at) {
    EventLock <RiderUpdatedETAPos> lock;

    auto [distance, _] = approx_eta(rider.eta_pos_, at);
    rider.eta_range_ -= distance;

    auto capacity_pct = rider.eta_range_ / sim::configs.fleet.max_range;
    auto approx_charge_time = static_cast<duration_t>(sim::configs.fleet.charge_time * capacity_pct);

    // go to charging location
    push_waypoints(rider, Waypoint{at, 0, INVALID_REQ_ID, Waypoint::Kind::ChargeStart},
                   Waypoint{at, approx_charge_time, INVALID_REQ_ID, Waypoint::Kind::ChargeDone});

    rider.eta_range_ = sim::configs.fleet.max_range;  // we will be back to full capacity once charging is complete
}
