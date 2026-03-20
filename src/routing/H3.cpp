#include "H3.h"
#include "h3api.h"


static constexpr int H3_RESOLUTION = 8;

struct NeighborHexCacheEntry {
    int computed_radius = -1;
    std::vector<hex_id_t> hexes;
};

thread_local std::unordered_map<hex_id_t, NeighborHexCacheEntry> neighbor_hex_cache_;

uint64_t latlon_to_h3(coordinate c) {
    LatLng g{c.lat, c.lon};

    H3Index res;
    latLngToCell(&g, H3_RESOLUTION, &res);
    return res;
}

const std::vector<hex_id_t> &hexes_in_increasing_radius(hex_id_t origin, int max_radius) {
    assert(max_radius >= 0 && "max_radius must be >= 0");

    auto &entry = neighbor_hex_cache_[origin];

    if (entry.computed_radius < 0) {
        entry.computed_radius = 0;
        entry.hexes.push_back(origin);
    }

    while (entry.computed_radius < max_radius) {
        int next_r = entry.computed_radius + 1;

        std::vector<H3Index> ring(6 * next_r, 0);
        gridRingUnsafe(origin, next_r, ring.data());

        entry.hexes.insert(entry.hexes.end(), ring.begin(), ring.end());
        entry.computed_radius = next_r;
    }

    return neighbor_hex_cache_.at(origin).hexes;
}
