#include "RiderPAX.h"
#include "riders/Rider.h"

#include <random>


RiderPAX::RiderPAX() {
    auto frac_2_seater = sim::configs.get<float>("frac_2_seater", 0.0);
    auto frac_4_seater = sim::configs.get<float>("frac_4_seater", 0.0);
    auto frac_6_seater = 1 - frac_2_seater - frac_4_seater;

    if (frac_2_seater < 0 or frac_2_seater > 1 or frac_6_seater < 0 or frac_6_seater > 1)
        throw std::runtime_error("pax proportions must be in [0, 1]");

    rider_id_to_capacity_.resize(sim::riders.size());

    std::mt19937 rng(std::random_device{}());
    std::discrete_distribution<int> dist{
            frac_2_seater,
            frac_4_seater,
            frac_6_seater
    };

    for (size_t i = 0; i < sim::riders.size(); i++) {
        switch (dist(rng)) {
            case 0:
                rider_id_to_capacity_[i] = 2;
                break;
            case 1:
                rider_id_to_capacity_[i] = 4;
                break;
            case 2:
                rider_id_to_capacity_[i] = 6;
                break;
        }
    }
}

uint8_t RiderPAX::capacity(rider_id_t rider_id) const {
    return rider_id_to_capacity_[rider_id];
}
