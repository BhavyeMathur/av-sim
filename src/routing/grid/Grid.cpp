#include "Grid.h"
#include "io/SimulationConfigs.h"

namespace sim {
    extern float cos_ref_lat;
}

namespace grid {
    absl::flat_hash_map<cell_id_t, NeighborCellCacheEntry> neighbor_cell_cache_;

    void init() {
        sim::cos_ref_lat = sim::configs.sim.cos_ref_lat;
        rect::cell_lon_rad = rect::cell_lat_rad / sim::cos_ref_lat;
    }

    cell_id_t latlon_to_cell(coordinate c) noexcept(use_fast_rect_grid_v) {
        if constexpr (use_fast_rect_grid_v)
            return rect::latlon_to_cell(c);
        else
            return h3::latlon_to_cell(c);
    }

    coordinate cell_to_latlon(cell_id_t h) noexcept(use_fast_rect_grid_v) {
        if constexpr (use_fast_rect_grid_v)
            return rect::cell_to_latlon(h);
        else
            return h3::cell_to_latlon(h);
    }

    const std::vector<cell_id_t> &cells_in_increasing_radius(cell_id_t origin, int max_radius) {
        if constexpr (use_fast_rect_grid_v)
            return rect::cells_in_increasing_radius(origin, max_radius);
        else
            return h3::cells_in_increasing_radius(origin, max_radius);
    }
}
