#pragma once

#include "includes.h"

#include "Order.h"


struct PingDataFrame {
    std::vector<order_id_t> id;
    std::vector<timestamp_t> created_at;

    std::vector<coordinate_t> pick_lat;
    std::vector<coordinate_t> pick_lon;
    std::vector<coordinate_t> drop_lat;
    std::vector<coordinate_t> drop_lon;

    std::vector<duration_t> pick_time;
    std::vector<duration_t> drop_time;
    std::vector<duration_t> ready_time;
    std::vector<duration_t> sla_time;

    std::vector<duration_t> predicted_ready_time;
    std::vector<distance_t> predicted_lm_dist;

    std::vector<zone_id_t> pick_zone;
    std::vector<zone_id_t> drop_zone;
    std::vector<customer_id_t> customer;

    explicit PingDataFrame(const std::string &file);

    [[nodiscard]] size_t size() const;

    struct iterator {
        const PingDataFrame *df;
        size_t index;

        using value_type = Order;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        iterator(const PingDataFrame *df_, size_t i);

        value_type operator*() const;

        iterator &operator++();

        iterator operator++(int);

        bool operator==(const iterator &other) const;

        bool operator!=(const iterator &other) const;
    };

    [[nodiscard]] iterator begin() const;

    [[nodiscard]] iterator end() const;
};
