#pragma once

#include <coordinate.h>
#include "absl/container/flat_hash_map.h"

namespace grid {
    constexpr bool use_fast_rect_grid_v = true;

    void init();

    namespace h3 {
        cell_id_t latlon_to_cell(coordinate c);

        coordinate cell_to_latlon(cell_id_t h);

        const std::vector<cell_id_t> &cells_in_increasing_radius(cell_id_t origin, int max_radius);
    }

    namespace rect {
        constexpr distance_t EarthRadius = 6371;

        constexpr double grid_cell_km = 0.8;
        constexpr double cell_lat_rad = grid_cell_km / EarthRadius;
        extern double cell_lon_rad;

        cell_id_t latlon_to_cell(coordinate c) noexcept;

        coordinate cell_to_latlon(cell_id_t h) noexcept;

        const std::vector<cell_id_t> &cells_in_increasing_radius(cell_id_t origin, int max_radius);
    }

    cell_id_t latlon_to_cell(coordinate c) noexcept(use_fast_rect_grid_v);

    coordinate cell_to_latlon(cell_id_t h) noexcept(use_fast_rect_grid_v);

    // returns [origin, ring1..., ring2..., ... , ring_max_radius...]
    const std::vector<cell_id_t> &cells_in_increasing_radius(cell_id_t origin, int max_radius);

    struct NeighborCellCacheEntry {
        int computed_radius = -1;
        std::vector<cell_id_t> cells;
        spinlock lock;
    };

    extern std::unordered_map<cell_id_t, NeighborCellCacheEntry> neighbor_cell_cache_;
    // extern absl::flat_hash_map<cell_id_t, NeighborCellCacheEntry> neighbor_cell_cache_;
}
