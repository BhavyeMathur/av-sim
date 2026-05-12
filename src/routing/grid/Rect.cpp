#include "Grid.h"

constexpr int32_t grid_bias = 1 << 20;

inline cell_id_t pack_rect_cell(int32_t i, int32_t j) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(i + grid_bias)) << 32) | static_cast<uint32_t>(j + grid_bias);
}

inline int32_t rect_i(cell_id_t id) noexcept {
    return static_cast<int32_t>(id >> 32) - grid_bias;
}

inline int32_t rect_j(cell_id_t id) noexcept {
    return static_cast<int32_t>(id & 0xffffffffu) - grid_bias;
}

spinlock cache_lock;

namespace grid::rect {
    double cell_lon_rad = 0;

    cell_id_t latlon_to_cell(coordinate c) noexcept {
        auto i = static_cast<int32_t>(std::floor(c.lat / cell_lat_rad));
        auto j = static_cast<int32_t>(std::floor(c.lon / cell_lon_rad));
        return pack_rect_cell(i, j);
    }

    coordinate cell_to_latlon(cell_id_t h) noexcept {
        auto i = rect_i(h);
        auto j = rect_j(h);
        return {static_cast<coordinate_t>((i + 0.5) * cell_lat_rad),
                static_cast<coordinate_t>((j + 0.5) * cell_lon_rad)};
    }

    const std::vector<cell_id_t> &cells_in_increasing_radius(cell_id_t origin, int max_radius) {
        assert(max_radius >= 0 && "max_radius must be >= 0");

        cache_lock.lock();
        auto &entry = neighbor_cell_cache_[origin];
        cache_lock.unlock();

        // TODO we should be able to precompute this!
        unique_spinlock lock(entry.lock);

        if (entry.computed_radius < 0) {
            entry.computed_radius = 0;
            entry.cells.reserve((2 * max_radius + 1) * (2 * max_radius + 1));
            entry.cells.push_back(origin);
        }

        auto ci = rect_i(origin);
        auto cj = rect_j(origin);

        while (entry.computed_radius < max_radius) {
            auto r = entry.computed_radius + 1;
            entry.cells.reserve((2 * max_radius + 1) * (2 * max_radius + 1));

            for (auto di = -r; di <= r; di++) {
                entry.cells.push_back(pack_rect_cell(ci + di, cj - r));
                entry.cells.push_back(pack_rect_cell(ci + di, cj + r));
            }

            for (auto dj = 1 - r; dj <= r - 1; dj++) {
                entry.cells.push_back(pack_rect_cell(ci - r, cj + dj));
                entry.cells.push_back(pack_rect_cell(ci + r, cj + dj));
            }
            entry.computed_radius = r;
        }

        return entry.cells;
    }
}
