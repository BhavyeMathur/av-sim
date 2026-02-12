#include "EVCharging.h"
#include "riders/Rider.h"
#include "io/SimulationConfigs.h"

namespace sim {
    extern thread_local SimulationConfigs configs;
}

EVCharging::EVCharging()
        : m_max_range(sim::configs.get<distance_t>("ev_charge_range")),
          m_charge_time(sim::configs.get<timestamp_t>("ev_charge_time")),
          m_charge_threshold_km(sim::configs.get<float>("ev_charge_threshold") * m_max_range),
          m_charge_to_km(sim::configs.get<float>("ev_charge_to") * m_max_range) {

}

EVCharging::EVCharging(bool) {}

void EVCharging::update(Rider &rider, distance_t distance) const {
    rider.range_km -= distance;

    if (rider.range_km < 0)
        throw std::runtime_error("Rider does not have enough range to service order!");

    if (rider.range_km <= m_charge_threshold_km) {
        auto range_to_charge = m_charge_to_km - rider.range_km;
        auto time_to_charge = static_cast<uint32_t>(m_charge_time * range_to_charge / m_max_range);

        rider.range_km = m_charge_to_km;
        rider.add_waypoint({0, time_to_charge, RiderWaypoint::SERVICE});
    }
}

distance_t EVCharging::start_with_charge() const noexcept {
    return m_charge_to_km;
}
