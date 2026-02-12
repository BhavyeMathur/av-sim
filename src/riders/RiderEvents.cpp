#include "RiderEvents.h"
#include "RiderPool.h"
#include "Rider.h"


RiderEvents RiderEvents::from_riders(RiderPool &riders) {
    std::vector<RiderEvent> logins;
    std::vector<RiderEvent> logouts;

    logins.reserve(riders.size());
    logouts.reserve(riders.size());

    for (const auto &rider: riders.all_riders()) {
        if (rider.eta_at == rider.logout_at)
            continue;

        assert(rider.eta_at < rider.logout_at);
        logins.push_back({rider.eta_at, rider.id});
        logouts.push_back({rider.logout_at, rider.id});
    }

    return {std::move(logins), std::move(logouts)};
}
