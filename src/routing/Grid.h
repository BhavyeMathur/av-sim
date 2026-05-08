#pragma once

#include <coordinate.h>

namespace grid {
    constexpr bool use_fast_rect_grid_v = true;

    void init();

    cell_id_t latlon_to_cell(coordinate c) noexcept(use_fast_rect_grid_v);

    coordinate cell_to_latlon(cell_id_t h) noexcept(use_fast_rect_grid_v);

    // returns [origin, ring1..., ring2..., ... , ring_max_radius...]
    const std::vector<cell_id_t> &cells_in_increasing_radius(cell_id_t origin, int max_radius);
}
