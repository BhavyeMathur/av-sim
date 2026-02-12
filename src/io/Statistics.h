#pragma once

#include "includes.h"


struct AllocationStatistic {
    std::optional<rider_id_t> rider_id{std::nullopt};
    order_id_t order_id;

    distance_t fm_dist;
    distance_t lm_dist;

    timestamp_t start_at;
    timestamp_t pickup_at;

    duration_t fm_time;
    duration_t wait_time;
    duration_t lm_time;
    duration_t drop_time;
};

class Statistics {
public:
    Statistics() = default;

    void resize(size_t n);

    void assign_order(const AllocationStatistic &result);

    void save(const std::string &filepath) const;

private:
    std::vector<bool> m_order_delivered;

    std::vector<distance_t> m_order_fm_dist;
    std::vector<distance_t> m_order_lm_dist;

    std::vector<timestamp_t> m_order_start_at;
    std::vector<duration_t> m_order_fm_time;
    std::vector<duration_t> m_order_wait_time;
    std::vector<duration_t> m_order_lm_time;
    std::vector<duration_t> m_order_drop_time;

    std::vector<rider_id_t> m_order_rider_id;
};
