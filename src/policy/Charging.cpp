#include "Charging.h"

#include "riders/Rider.h"
#include "extern.h"

ChargingPolicy::ChargingPolicy() {
    sim::events.on<&ChargingPolicy::on_rider_updated_eta_pos>(*this);
}

void ChargeInPlace::on_rider_updated_eta_pos(const RiderUpdatedETAPos &e) {
    static constexpr distance_t minimum_ = 0.2 * 300;   // 60 km

    auto &state = sim::rider_battery.state(e.rider_id);
    if (state.eta_range_ > minimum_)
        return;

    auto &rider = sim::riders[e.rider_id];
    sim::rider_battery.charge(rider, rider.eta_pos());
}

void ChargeAtHub::on_rider_updated_eta_pos(const RiderUpdatedETAPos &e) {
    static constexpr distance_t minimum_ = 0.2 * 300;   // 60 km

    auto &state = sim::rider_battery.state(e.rider_id);
    if (state.eta_range_ > minimum_)
        return;

    auto &rider = sim::riders[e.rider_id];

    // TODO make this an input file/geo file
    //    static const std::array<coordinate, 5> chargers = {
    //            {0.737868043395543,  -1.4614804806035193},
    //            {0.7383711790474249, -1.461022762884997},
    //            {0.7373253693979914, -1.4617249397298056},
    //            {0.7374271558957626, -1.4607404343551826},
    //            {0.7378009581918764, -1.4621699204336844}
    //    };

    //    static const std::array<coordinate, 1> chargers = {
    //            {{0.7378823022556971, -1.4614832306793801}},
    //    };

    static const std::array<coordinate, 2> chargers = {
            {{0.7376146380678315, -1.4615968275440983},
             {0.7380814594589727, -1.4611157745557037}},
    };

    duration_t best_time = std::numeric_limits<duration_t>::max();
    size_t best_charger = -1;

    #pragma unroll
    for (size_t i = 0; i < chargers.size(); i++) {
        auto [_, time] = approx_eta(rider.eta_pos(), chargers[i]);

        if (time < best_time) {
            best_time = time;
            best_charger = i;
        }
    }

    sim::rider_battery.charge(rider, chargers[best_charger]);
}
