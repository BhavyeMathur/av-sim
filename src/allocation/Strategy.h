#pragma once

#include "includes.h"

#include "Request.h"
#include "riders/Rider.h"
#include "riders/RiderBattery.h"
#include "riders/RiderPAX.h"

#include "routing/Distance.h"
#include "RiderSource.h"


class BaseStrategy {
public:
    struct RiderInfo {
        Rider *rider = nullptr;
        uint8_t pax = std::numeric_limits<uint8_t>::max();
        distance_t fm_dist_km = std::numeric_limits<distance_t>::max();
    };

protected:
    RiderBattery rider_battery;
    RiderPAX rider_pax;

    BaseStrategy();

    [[nodiscard]] bool is_rider_feasible(const Rider &rider, const Request &request, RiderInfo &info);

private:
    virtual rider_id_t match(const Request &request) = 0;

    void on_request(const RequestCreated &event);
};

template<class Derived>
    class SequentialStrategy : public BaseStrategy {
    private:
        rider_id_t match(const Request &request) override {
            typename Derived::RiderInfo best;

            for (auto &pool: static_cast<Derived *>(this)->candidate_pools(request)) {
                for (auto rider_id: pool) {
                    auto &rider = sim::riders[rider_id];

                    typename Derived::RiderInfo info;
                    info.rider = &rider;

                    if (!is_rider_feasible(rider, request, info))
                        continue;

                    if (static_cast<Derived *>(this)->is_better(info, best))
                        best = info;
                }

                if (best.rider)
                    return best.rider->id();
            }

            return INVALID_RIDER_ID;
        }
    };
