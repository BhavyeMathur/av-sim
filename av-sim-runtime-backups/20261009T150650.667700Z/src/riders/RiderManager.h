#pragma once

#include "includes.h"

#include "Rider.h"
#include "RiderGrid.h"
#include "events/EventBus.h"

struct RidersDataFrame;
struct Waypoint;

class RiderManager : public RiderGrid {
public:
    RiderManager() = default;

    explicit RiderManager(const RidersDataFrame &df);

    [[nodiscard]] auto size() const { return n_riders_; }

    [[nodiscard]] RiderData &get_data(rider_id_t rider_id);

    template<typename... W> requires (std::same_as<std::decay_t<W>, Waypoint> &&...)

    auto push_waypoints(rider_id_t rider_id, W &&... wp) {
        auto &rider = get_rider(rider_id);
        return push_waypoints(rider, wp...);
    }

    template<typename... W> requires (std::same_as<std::decay_t<W>, Waypoint> &&...)

    void push_waypoints(Rider &rider, W &&... wp) {
        static_assert(sizeof...(W) > 0);
        if (rider.state_ == RiderState::Dead)
            throw std::runtime_error("cannot push waypoint to dead rider");

        auto rider_id = rider.id();
        auto &rider_data = get_data(rider_id);
        distance_t total_distance = 0;
        coordinate last_pos{};

        auto process = [&](const Waypoint &w) {
            auto [distance, duration] = approx_eta(last_pos, w.pos);
            rider_data.steps_.push_back({w, duration});
            rider.eta_at_ = std::max(sim::clock, rider.eta_at_) + duration + w.dwell_s;

            total_distance += distance;
            last_pos = w.pos;
        };
        (process(wp), ...);

        rider.eta_range_ -= total_distance;
        rider.eta_pos_ = last_pos;
        rider = update(rider);
        sim::events.trigger(RiderUpdatedETAPos{rider_id, total_distance});

        if (!rider.next_scheduled_)
            rider.schedule_next_(rider_data);
    }

    void charge(rider_id_t rider_id, coordinate at);

    void charge(Rider &rider, coordinate at);

private:
    std::vector<RiderData> rider_id_to_data_;
    size_t n_riders_ = 0;
};
