#pragma once

#include "includes.h"
#include "RiderData.h"

struct RiderStateChange;

struct SimComplete;


class FleetStats {
public:
    FleetStats();

private:
    // statistics
    std::array<uint32_t, static_cast<size_t>(RiderState::SIZE)> _n_in_state = {0};

    std::vector<timestamp_t> _timestamps;
    std::array<std::vector<uint32_t>, static_cast<size_t>(RiderState::SIZE)> _n_in_state_vs_t;

    // logging
    duration_t _log_interval;
    timestamp_t _last_log_at = 0;

    void _log();

    void _on_rider_state_change(const RiderStateChange &e);

    void _on_sim_complete(const SimComplete &e);
};
