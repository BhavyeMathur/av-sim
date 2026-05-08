#pragma once

#include "includes.h"

class RiderHexIndex {
public:
    using rider_set_t = std::vector<rider_id_t>;

    explicit RiderHexIndex(size_t n_riders);

    [[nodiscard]] const rider_set_t &riders_in_hex(cell_id_t hex) const;

private:
    void on_rider_updated_eta_pos(const RiderUpdatedETAPos &event);

    void on_request_completed(const RequestCompleted &event);

    void update(rider_id_t rider_id);

private:
    std::vector<cell_id_t> rider_id_to_hex_id_;
    std::vector<uint32_t> rider_id_to_pos_;

    std::unordered_map<cell_id_t, rider_set_t> hex_id_to_riders_;
};
