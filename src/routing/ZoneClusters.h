#pragma once

#include "includes.h"

struct Zone {
    cluster_id_t cluster;
    Coordinate centroid;

    Zone() = default;
};

class ZoneClusters {
public:
    ZoneClusters();

    explicit ZoneClusters(bool dont_load);

    [[nodiscard]] bool is_deliverable(zone_id_t rider_zone, zone_id_t drop_zone) const;

    [[nodiscard]] Zone operator[](zone_id_t zone_id) const;

private:
    friend std::istream &operator>>(std::istream &in, Zone &zone);

    std::unordered_map<zone_id_t, Zone> m_zone_to_cluster;
    std::unordered_map<cluster_id_t, std::unordered_set<zone_id_t>> m_cluster_to_zones;
};

std::istream &operator>>(std::istream &in, Zone &zone);
