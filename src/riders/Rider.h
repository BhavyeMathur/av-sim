#pragma once

#include "RiderData.h"
#include "routing/Routing.h"

class Rider {
public:
    friend class RiderManager;

    Rider(rider_id_t id, coordinate pos, uint8_t pax);

    [[nodiscard]] auto id() const { return id_; }

    [[nodiscard]] auto eta_pos() const { return eta_pos_; }

    [[nodiscard]] auto eta_at() const { return eta_at_; }

    [[nodiscard]] auto eta_range() const { return eta_range_; }

    [[nodiscard]] auto state() const { return state_; }

    [[nodiscard]] auto n_requests_assigned() const { return n_assigned_; }

    [[nodiscard]] auto capacity() const { return capacity_; }

    void assign_request();

    void complete_waypoint();

    static std::string state_to_string(RiderState state);

private:
    rider_id_t id_;

    coordinate eta_pos_{};
    timestamp_t eta_at_ = 0;

    // charging/range related variables
    distance_t eta_range_;

    RiderState state_ = RiderState::Idle;
    uint8_t n_assigned_ = 0;
    uint8_t capacity_;
    bool next_scheduled_ = false;  // next completion scheduling guard

    void schedule_next_(RiderData &data);

    void recalculate_eta_at_(RiderData &data);

    void set_state_if_not_dead_(RiderState state);
};
