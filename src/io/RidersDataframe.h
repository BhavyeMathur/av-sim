#pragma once

#include "includes.h"


struct RidersDataFrame {
    struct Rider {
        float lat;
        float lon;
    };

    std::vector<float> lat;
    std::vector<float> lon;

    explicit RidersDataFrame(const std::string &file);

    [[nodiscard]] size_t size() const;

    struct iterator {
        const RidersDataFrame *df;
        size_t index;

        using value_type = Rider;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;

        iterator(const RidersDataFrame *df_, size_t i);

        value_type operator*() const;

        iterator &operator++();

        iterator operator++(int);

        bool operator==(const iterator &other) const;

        bool operator!=(const iterator &other) const;
    };

    [[nodiscard]] iterator begin() const;

    [[nodiscard]] iterator end() const;
};
