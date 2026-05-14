#pragma once

#include "includes.h"


struct RiderPool;

class RidersIterator {
public:
    explicit RidersIterator(uint8_t max_radius = 3) : max_radius_(max_radius) {}

    class Range {
    public:
        class iterator {
        public:
            using iterator_category = std::input_iterator_tag;
            using iterator_concept = std::input_iterator_tag;
            using value_type = RiderPool;
            using difference_type = std::ptrdiff_t;

            iterator() = default;

            iterator(const std::vector<cell_id_t> *cells, size_t pos = 0)
                    : cells_(cells), cell_pos_(pos) {}

            RiderPool &operator*();

            iterator &operator++() {
                ++cell_pos_;
                return *this;
            }

            iterator operator++(int) {
                auto tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const iterator &other) const = default;

        private:
            const std::vector<cell_id_t> *cells_ = nullptr;
            size_t cell_pos_ = 0;
        };

        Range(const std::vector<cell_id_t> &cells) : cells_(cells) {}

        [[nodiscard]] iterator begin() { return {&cells_}; }

        [[nodiscard]] iterator end() { return {&cells_, cells_.size()}; }

    private:
        const std::vector<cell_id_t> &cells_;
    };

    [[nodiscard]] Range candidate_pools(const Request &req);

private:
    int max_radius_;
};
