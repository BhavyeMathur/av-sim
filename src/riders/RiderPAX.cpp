#include "RiderPAX.h"
#include "riders/Rider.h"

#include <random>


void RiderPAX::init() {
    rider_id_to_capacity_.resize(sim::n_riders);

    std::mt19937 rng(std::random_device{}());
    std::discrete_distribution<int> dist{
            sim::configs.fleet.frac_2_seater,
            sim::configs.fleet.frac_4_seater,
            sim::configs.fleet.frac_6_seater
    };

    for (size_t i = 0; i < sim::n_riders; i++) {
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
