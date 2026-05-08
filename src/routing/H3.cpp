#include "H3.h"
#include "h3api.h"


struct NeighborHexCacheEntry {
    int computed_radius = -1;
    std::vector<hex_id_t> hexes;
};

struct H3CellCacheEntry {
    coordinate centroid;
};

std::unordered_map<hex_id_t, NeighborHexCacheEntry> neighbor_hex_cache_;
std::unordered_map<hex_id_t, H3CellCacheEntry> h3_cell_cache_;

hex_id_t latlon_to_h3(coordinate c) {
    LatLng g{c.lat, c.lon};

    H3Index res;
    latLngToCell(&g, sim::configs.sim.h3_resolution, &res);
    return res;
}

coordinate h3_to_latlon(hex_id_t h) {
    auto [it, inserted] = h3_cell_cache_.try_emplace(h);

    if (inserted) {
        LatLng g;
        cellToLatLng(static_cast<H3Index>(h), &g);
        it->second.centroid = {static_cast<coordinate_t>(g.lat), static_cast<coordinate_t>(g.lng)};
    }

    return it->second.centroid;
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
