#include "Hotspots.h"

#include <fstream>


namespace sim {
    extern thread_local ZoneClusters zones;
}

HotspotEstimator::HotspotEstimator(duration_t window, duration_t resolution, float threshold)
        : m_window(window),
          m_resolution(resolution),
          m_threshold(threshold) {
}

void HotspotEstimator::up(zone_id_t zone_id) {
    if (!m_zone_to_estimator.contains(zone_id))
        m_zone_to_estimator.insert({zone_id, {m_window, m_resolution}});
    m_zone_to_estimator.at(zone_id).up();
}

void HotspotEstimator::down(zone_id_t zone_id) {
    if (!m_zone_to_estimator.contains(zone_id))
        return;
    m_zone_to_estimator.at(zone_id).down();
}

void HotspotEstimator::update() {
    m_hotspots.clear();

    uint32_t max_count = 0;
    for (const auto &[_, estimator]: m_zone_to_estimator)
        max_count = std::max(max_count, estimator.count());

    if (max_count < 10)
        return;

    auto threshold = static_cast<uint32_t>(m_threshold * static_cast<float>(max_count));

    for (const auto &[zone_id, estimator]: m_zone_to_estimator)
        if (estimator.count() > threshold)
            m_hotspots.push_back(sim::zones[zone_id]);
}

Zone HotspotEstimator::find_closest_to(Coordinate coord) const {
    auto min_dist = std::numeric_limits<distance_t>::max();
    Zone result{};

    for (auto hotspot : m_hotspots) {
        auto dist = hotspot.centroid.distance_to(coord);
        if (dist < min_dist) {
            result = hotspot;
            min_dist = dist;
        }
    }

    return result;
}
