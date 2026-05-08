#pragma once

#include "includes.h"
#include "absl/container/flat_hash_map.h"

class RiderGrid {
public:
    using rider_set_t = std::vector<rider_id_t>;

    explicit RiderGrid(size_t n_riders);

    [[nodiscard]] const rider_set_t &riders_in_cell(cell_id_t cell) const;

private:
    void on_rider_updated_eta_pos(const RiderUpdatedETAPos &event);

    void on_request_completed(const RequestCompleted &event);

    void update(rider_id_t rider_id);

private:
    std::vector<cell_id_t> rider_id_to_cell_;
    std::vector<uint32_t> rider_id_to_pos_;

    absl::flat_hash_map<cell_id_t, rider_set_t> cell_to_riders_;
};
