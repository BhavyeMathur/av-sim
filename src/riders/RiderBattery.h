#pragma once

#include <coordinate.h>
#include "includes.h"


class RiderBattery {
public:
    struct State {
        distance_t eta_range_ = capacity_;
        distance_t range_ = capacity_;
        bool charging_scheduled_ = false;
    };

    void init();

    void charge(Rider &rider, coordinate at);

    [[nodiscard]] bool check_capacity(rider_id_t rider_id, distance_t distance = 0) const;

    [[nodiscard]] const State &state(rider_id_t rider_id) const;

private:
    static constexpr distance_t capacity_ = 0.8 * 300;  // 240 km
    static constexpr duration_t charge_time_ = 3600;    // 1 hour

    std::vector<State> rider_id_to_state_;

    void on_update_rider_eta_pos_(const RiderUpdatedETAPos &e);

    void on_rider_schedule_waypoint(const RiderScheduleWaypoint &e);

    void on_rider_charge_start_(const RiderChargeStart &e);

    void on_rider_charge_complete_(const RiderChargeComplete &e);
};
