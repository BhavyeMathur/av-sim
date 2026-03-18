#pragma once

#include "includes.h"


class RiderBattery {
public:
    void init();

    [[nodiscard]] bool check_capacity(rider_id_t rider_id, distance_t distance = 0) const;

    void charge(Rider &rider) const;

private:
    static constexpr distance_t capacity_ = 0.8 * 300;  // 240 km
    static constexpr distance_t minimum_ = 0.2 * 300;   // 60 km
    static constexpr duration_t charge_time_ = 3600;    // 1 hour

    struct State {
        distance_t eta_range_ = capacity_;
        distance_t range_ = capacity_;
    };

    std::vector<State> rider_id_to_state_;

    void on_update_rider_eta_pos_(const RiderUpdatedETAPos &e);

    void on_rider_charge_complete_(const RiderChargeComplete &e);
};
