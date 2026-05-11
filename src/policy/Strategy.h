#pragma once

#include "includes.h"

#include "Request.h"
#include "riders/RiderBattery.h"
#include "riders/RiderPAX.h"
#include "riders/RiderSource.h"
#include "routing/Distance.h"
#include "events/EventBus.h"

namespace sim {
    extern EventBus events;

    extern RiderBattery rider_battery;
    extern RiderPAX rider_pax;
}

class Strategy {
public:
    // the base strategy contains a struct "RiderInfo" which is computed every time is_rider_feasible is called
    // the struct may be partially initialized when is_rider_feasible returns false.
    struct RiderInfo {
        Rider *rider = nullptr;
        uint8_t pax = std::numeric_limits<uint8_t>::max();
        distance_t fm_dist_km = std::numeric_limits<distance_t>::max();
    };

    Strategy();

    virtual ~Strategy() = default;

protected:
    // checks for global feasibility parameters such as
    //  1. passenger capacity (pax)
    //  2. battery life
    template<bool check_pax = true>
        [[nodiscard]] static bool is_rider_feasible(const Rider &rider, const Request &request, RiderInfo &cand) {
            if constexpr (check_pax) {
                cand.pax = sim::rider_pax.capacity(rider.id());
                if (cand.pax < request.pax)
                    return false;
            }

            cand.fm_dist_km = sim::distance(rider.eta_pos(), request.pick_coord);
            if (!sim::rider_battery.check_capacity(rider.id(), cand.fm_dist_km + request.predicted_lm_dist))
                return false;

            return true;
        }

protected:
    static constexpr float speed_kmps = 40.0 / 3600;

private:
    virtual void assign_request(const Request &request) = 0;

    void on_request(const RequestCreated &event);
};
