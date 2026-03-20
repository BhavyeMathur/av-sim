#define DEBUG false

#include "FleetStats.h"

#include <pandas.h>


FleetStats::FleetStats()
        : _log_interval(sim::configs.get<duration_t>("fleet_stats_log_interval", 900)) {

    _n_in_state[static_cast<uint8_t>(Rider::State::Idle)] = sim::riders.size();

    sim::events.on<&FleetStats::_on_rider_state_change>(*this);
    sim::events.on<&FleetStats::_on_sim_complete>(*this);
}

void FleetStats::_log() {
    if (sim::clock < _log_interval + _last_log_at)
        return;

    _last_log_at = sim::clock;
    _timestamps.push_back(sim::clock);

    #pragma unroll
    for (uint8_t i = 0; i < static_cast<uint8_t>(Rider::State::SIZE); i++)
        _n_in_state_vs_t[i].push_back(_n_in_state[i]);
}

void FleetStats::_on_rider_state_change(const RiderStateChange &e) {
    auto old_state = static_cast<uint8_t>(e.old_state);

    assert(_n_in_state[old_state] >= 1);
    _n_in_state[old_state]--;
    _n_in_state[static_cast<uint8_t>(e.new_state)]++;

    _log();
}

void FleetStats::_on_sim_complete(const SimComplete &) {
    printf("...saving fleet statistics (count=%zu)\n", _timestamps.size());

    std::vector<pd::AnyColumn> cols;
    cols.reserve(1 + static_cast<uint8_t>(Rider::State::SIZE));

    // TODO change name from col_dynamic to col
    cols.push_back(pd::col_dynamic("timestamp", _timestamps));

    for (uint8_t i = 0; i < static_cast<uint8_t>(Rider::State::SIZE); ++i)
        cols.push_back(pd::col_dynamic(
                Rider::state_to_string(static_cast<Rider::State>(i)),
                _n_in_state_vs_t[i]
        ));

    auto table = pd::make_table(cols).ValueOrDie();

    auto filepath = sim::configs.get<std::string>("output") + "-fleet.parquet";
    if (!pd::write_table_to_parquet(table, filepath).ok())
        throw std::runtime_error("Failed to write the output file to " + filepath);
}
