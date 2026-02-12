#pragma once

#include "includes.h"
#include "util/rolling_counter.h"
#include "ZoneClusters.h"


class HotspotEstimator {
public:
    HotspotEstimator(duration_t window, duration_t resolution, float threshold);

    void up(zone_id_t zone_id);

    void down(zone_id_t zone_id);

    void update();

    [[nodiscard]] Zone find_closest_to(Coordinate coord) const;

private:
    duration_t m_window;
    duration_t m_resolution;
    float m_threshold;

    std::unordered_map<zone_id_t, rolling_counter> m_zone_to_estimator;
    std::vector<Zone> m_hotspots;
};
