#include "Grid.h"
#include "h3api.h"

namespace sim {
    extern float cos_ref_lat;
}

constexpr distance_t EarthRadius = 6371;

struct NeighborCellCacheEntry {
    int computed_radius = -1;
    std::vector<cell_id_t> cells;
};

struct H3CellCacheEntry {
    coordinate centroid;
};

std::unordered_map<cell_id_t, NeighborCellCacheEntry> neighbor_cell_cache_;
std::unordered_map<cell_id_t, H3CellCacheEntry> h3_cell_cache_;

cell_id_t latlon_to_h3(coordinate c) {
    LatLng g{c.lat, c.lon};

    H3Index res;
    latLngToCell(&g, sim::configs.sim.h3_resolution, &res);
    return res;
}

coordinate h3_to_latlon(cell_id_t h) {
    auto [it, inserted] = h3_cell_cache_.try_emplace(h);

    if (inserted) {
        LatLng g;
        cellToLatLng(static_cast<H3Index>(h), &g);
        it->second.centroid = {static_cast<coordinate_t>(g.lat), static_cast<coordinate_t>(g.lng)};
    }

    return it->second.centroid;
}

const std::vector<cell_id_t> &h3_cells_in_increasing_radius(cell_id_t origin, int max_radius) {
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

constexpr double grid_cell_km = 0.8;
constexpr double grid_cell_lat_rad = grid_cell_km / EarthRadius;
constexpr int32_t grid_bias = 1 << 20;
double grid_cell_lon_rad = 0;

inline cell_id_t pack_rect_cell(int32_t i, int32_t j) noexcept {
    return (static_cast<uint64_t>(static_cast<uint32_t>(i + grid_bias)) << 32) | static_cast<uint32_t>(j + grid_bias);
}

inline int32_t rect_i(cell_id_t id) noexcept {
    return static_cast<int32_t>(id >> 32) - grid_bias;
}

inline int32_t rect_j(cell_id_t id) noexcept {
    return static_cast<int32_t>(id & 0xffffffffu) - grid_bias;
}

inline cell_id_t latlon_to_rect(coordinate c) noexcept {
    auto i = static_cast<int32_t>(std::floor(c.lat / grid_cell_lat_rad));
    auto j = static_cast<int32_t>(std::floor(c.lon / grid_cell_lon_rad));
    return pack_rect_cell(i, j);
}

inline coordinate rect_to_latlon(cell_id_t h) noexcept {
    auto i = rect_i(h);
    auto j = rect_j(h);
    return {static_cast<coordinate_t>((i + 0.5) * grid_cell_lat_rad),
            static_cast<coordinate_t>((j + 0.5) * grid_cell_lon_rad)};
}

const std::vector<cell_id_t> &rect_cells_in_increasing_radius(cell_id_t origin, int max_radius) {
    assert(max_radius >= 0 && "max_radius must be >= 0");

    auto &entry = neighbor_cell_cache_[origin];

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

namespace grid {
    void init() {
        sim::cos_ref_lat = sim::configs.sim.cos_ref_lat;
        grid_cell_lon_rad = grid_cell_lat_rad / sim::cos_ref_lat;
    }

    cell_id_t latlon_to_cell(coordinate c) noexcept(use_fast_rect_grid_v) {
        if constexpr (use_fast_rect_grid_v)
            return latlon_to_rect(c);
        else
            return latlon_to_h3(c);
    }

    coordinate cell_to_latlon(cell_id_t h) noexcept(use_fast_rect_grid_v) {
        if constexpr (use_fast_rect_grid_v)
            return rect_to_latlon(h);
        else
            return h3_to_latlon(h);
    }

    const std::vector<cell_id_t> &cells_in_increasing_radius(cell_id_t origin, int max_radius) {
        if constexpr (use_fast_rect_grid_v)
            return rect_cells_in_increasing_radius(origin, max_radius);
        else
            return h3_cells_in_increasing_radius(origin, max_radius);
    }
}
