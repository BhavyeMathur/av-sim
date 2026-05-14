#include "Charging.h"
#include "riders/Rider.h"
#include "riders/RiderManager.h"
#include "events/EventBus.h"
#include "io/SimulationConfigs.h"

#include <pandas.h>

ChargingPolicy::ChargingPolicy() {
    sim::events.on<&ChargingPolicy::on_rider_updated_eta_pos>(*this);
}

void ChargeInPlace::on_rider_updated_eta_pos(const RiderUpdatedETAPos &e) {
    static constexpr distance_t minimum_ = 0.2 * 300;   // 60 km

    auto &rider = sim::riders.get_rider(e.rider_id);
    if (rider.eta_range() <= minimum_)
        sim::riders.charge(rider, rider.eta_pos());
}

ChargeAtHub::ChargeAtHub() {
    auto hubs = pd::read_csv(sim::configs.policy.charging_hubs);

    auto lat = pd::column_as_vector<pd::float64>(hubs, "lat");
    auto lon = pd::column_as_vector<pd::float64>(hubs, "lon");
    auto n_hubs = lat.size();

    hubs_.reserve(n_hubs);
    for (size_t i = 0; i < n_hubs; i++)
        hubs_.emplace_back(coordinate(lat[i], lon[i]).radians());
}

void ChargeAtHub::on_rider_updated_eta_pos(const RiderUpdatedETAPos &e) {
    static constexpr distance_t minimum_ = 0.2 * 300;   // 60 km

    auto &rider = sim::riders.get_rider(e.rider_id);
    if (rider.eta_range() > minimum_)
        return;

    duration_t best_time = std::numeric_limits<duration_t>::max();
    coordinate best_hub{};

    for (auto hub: hubs_) {
        auto [_, time] = approx_eta(rider.eta_pos(), hub);

        if (time < best_time) {
            best_time = time;
            best_hub = hub;
        }
    }

    sim::riders.charge(rider, best_hub);
}
