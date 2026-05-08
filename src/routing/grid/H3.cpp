#include "Grid.h"
#include "h3api.h"

struct H3CellCacheEntry {
    coordinate centroid;
};

std::unordered_map<cell_id_t, H3CellCacheEntry> h3_cell_cache_;

namespace grid::h3 {
    cell_id_t latlon_to_cell(coordinate c) {
        LatLng g{c.lat, c.lon};

        H3Index res;
        latLngToCell(&g, sim::configs.sim.h3_resolution, &res);
        return res;
    }

    coordinate cell_to_latlon(cell_id_t h) {
        auto [it, inserted] = h3_cell_cache_.try_emplace(h);

        if (inserted) {
            LatLng g;
            cellToLatLng(static_cast<H3Index>(h), &g);
            it->second.centroid = {static_cast<coordinate_t>(g.lat), static_cast<coordinate_t>(g.lng)};
        }

        return it->second.centroid;
    }

    const std::vector<cell_id_t> &cells_in_increasing_radius(cell_id_t origin, int max_radius) {
        assert(max_radius >= 0 && "max_radius must be >= 0");

        auto &entry = neighbor_cell_cache_[origin];

        if (entry.computed_radius < 0) {
            entry.computed_radius = 0;
            entry.cells.push_back(origin);
        }

        while (entry.computed_radius < max_radius) {
            const int next_r = entry.computed_radius + 1;

            const auto old_size = entry.cells.size();
            entry.cells.resize(old_size + 6 * next_r);

            gridRingUnsafe(static_cast<H3Index>(origin), next_r,
                           reinterpret_cast<H3Index *>(entry.cells.data() + old_size));
            entry.computed_radius = next_r;
        }

        return entry.cells;
    }
}
