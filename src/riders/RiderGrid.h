#pragma once

#include "includes.h"
#include "absl/container/flat_hash_map.h"

class RiderGrid {
public:
    struct Pool {
        std::vector<rider_id_t> riders{};
        cell_id_t cell;

        explicit Pool(cell_id_t cell) : cell(cell) {}
    };

    using rider_set_t = Pool;

    explicit RiderGrid(size_t n_riders);

    [[nodiscard]] FORCE_INLINE const rider_set_t &riders_in_cell(cell_id_t cell) const {
        auto it = cell_to_riders_.find(cell);
        if (it != cell_to_riders_.end())
            return it->second;

        static RiderGrid::rider_set_t s{0};
        s.cell = cell;
        return s;
    }

private:
    void on_rider_updated_eta_pos(const RiderUpdatedETAPos &event);

    void on_request_completed(const RequestCompleted &event);

    void update(rider_id_t rider_id);

private:
    std::vector<cell_id_t> rider_id_to_cell_;
    std::vector<uint32_t> rider_id_to_pos_;

    absl::flat_hash_map<cell_id_t, rider_set_t> cell_to_riders_;
};
