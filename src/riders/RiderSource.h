#pragma once

#include "includes.h"
#include "riders/RiderGrid.h"
#include "riders/Rider.h"


class AllRidersSource {
public:
    using pool_t = std::vector<rider_id_t>;

    AllRidersSource();

    [[nodiscard]] const std::array<pool_t, 1> &candidate_pools(const Request &) const { return rider_ids_; }

private:
    std::array<std::vector<rider_id_t>, 1> rider_ids_;
};

class CellRidersSource {
public:
    explicit CellRidersSource(uint8_t max_radius = 3)
            : index_(sim::n_riders),
              max_radius_(max_radius) {}

    class Range {
    public:
        class iterator {
        public:
            using iterator_category = std::input_iterator_tag;
            using iterator_concept = std::input_iterator_tag;
            using value_type = RiderGrid::rider_set_t;
            using difference_type = std::ptrdiff_t;

            iterator() = default;

            iterator(const RiderGrid *index, const std::vector<cell_id_t> *cells, size_t pos = 0)
                    : index_(index),
                      cells_(cells),
                      cell_pos_(pos) {}

            const RiderGrid::rider_set_t &operator*() const {
                auto cell = (*cells_)[cell_pos_];
                return index_->riders_in_cell(cell);
            }

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
            const RiderGrid *index_ = nullptr;
            const std::vector<cell_id_t> *cells_ = nullptr;
            size_t cell_pos_ = 0;
        };

        Range(const RiderGrid &index, const std::vector<cell_id_t> &cells)
                : index_(index), cells_(cells) {}

        [[nodiscard]] iterator begin() const { return {&index_, &cells_}; }

        [[nodiscard]] iterator end() const { return {&index_, &cells_, cells_.size()}; }

    private:
        const RiderGrid &index_;
        const std::vector<cell_id_t> &cells_;
    };

    using pool_t = Range::iterator::value_type;

    [[nodiscard]] Range candidate_pools(const Request &req) const;

    spinlock &get_lock(cell_id_t cell) { return index_.get_lock(cell); }

private:
    RiderGrid index_;
    int max_radius_;
};
