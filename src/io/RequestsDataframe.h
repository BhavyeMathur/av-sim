#pragma once

#include "includes.h"
#include <coordinate.h>

#include "Request.h"


struct RequestsDataFrame {
    std::vector<order_id_t> id;
    std::vector<timestamp_t> created_at;

    std::vector<coordinate_t> pick_lat;
    std::vector<coordinate_t> pick_lon;
    std::vector<coordinate_t> drop_lat;
    std::vector<coordinate_t> drop_lon;

    std::vector<distance_t> predicted_lm_dist;

    explicit RequestsDataFrame(const std::string &file);

    [[nodiscard]] size_t size() const;

    struct iterator {
        const RequestsDataFrame *df;
        size_t index;

        using value_type = Request;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        iterator(const RequestsDataFrame *df_, size_t i);

        value_type operator*() const;

        iterator &operator++();

        iterator operator++(int);

        bool operator==(const iterator &other) const;

        bool operator!=(const iterator &other) const;
    };

    [[nodiscard]] iterator begin() const;

    [[nodiscard]] iterator end() const;
};
