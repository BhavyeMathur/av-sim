#pragma once

#include "includes.h"
#include "absl/container/flat_hash_map.h"

struct RequestCompleted;

class Rider;

struct RiderPool {
    cell_id_t cell;
    std::vector<Rider> riders;
};

class RiderGrid {
public:
    explicit RiderGrid(size_t n_riders = 0);

    [[nodiscard]] RiderPool &riders_in_cell(cell_id_t cell);

    [[nodiscard]] Rider &get_rider(rider_id_t rider_id);

private:
    std::vector<cell_id_t> rider_id_to_cell_;
    std::vector<uint32_t> rider_id_to_idx_;

    absl::flat_hash_map<cell_id_t, RiderPool> cell_to_riders_;

protected:
    void emplace(rider_id_t id, coordinate pos, uint8_t pax);

    void on_request_completed(const RequestCompleted &event);

    Rider &update(Rider &rider);
};
