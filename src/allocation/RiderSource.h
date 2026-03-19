#pragma once

#include "includes.h"
#include "riders/RiderHex.h"

class AllRidersSource {
public:
    AllRidersSource();

    const std::vector<rider_id_t> &candidates(coordinate) { return rider_ids_; }

private:
    std::vector<rider_id_t> rider_ids_;
};

class HexRidersSource {
public:
    HexRidersSource(size_t n_riders, int max_radius)
            : index_(n_riders), max_radius_(max_radius) {}

    class Range {
    public:
        class iterator {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = rider_id_t;
            using difference_type = std::ptrdiff_t;

            iterator() = default;

            iterator(const RiderHexIndex *index, const std::vector<hex_id_t> *hexes, bool is_end)
                    : index_(index), hexes_(hexes), is_end_(is_end) {

                if (is_end_ or !index_ or !hexes_ or hexes_->empty()) {
                    is_end_ = true;
                    return;
                }

                hex_pos_ = 0;
                advance_to_next_nonempty_hex();
            }

            rider_id_t operator*() const {
                return *rider_it_;
            }

            iterator &operator++() {
                if (is_end_)
                    return *this;

                ++rider_it_;
                if (rider_it_ != rider_end_)
                    return *this;

                ++hex_pos_;
                advance_to_next_nonempty_hex();
                return *this;
            }

            bool operator==(const iterator &other) const {
                if (is_end_ && other.is_end_)
                    return true;

                return index_ == other.index_
                       and hexes_ == other.hexes_
                       and hex_pos_ == other.hex_pos_
                       and is_end_ == other.is_end_
                       and (is_end_ or rider_it_ == other.rider_it_);
            }

            bool operator!=(const iterator &other) const {
                return !(*this == other);
            }

        private:
            void advance_to_next_nonempty_hex() {
                while (hexes_ && hex_pos_ < hexes_->size()) {
                    const auto hex = (*hexes_)[hex_pos_];
                    const auto &riders = index_->riders_in_hex(hex);

                    rider_it_ = riders.begin();
                    rider_end_ = riders.end();

                    if (rider_it_ != rider_end_) {
                        is_end_ = false;
                        return;
                    }

                    ++hex_pos_;
                }

                is_end_ = true;
            }

        private:
            const RiderHexIndex *index_ = nullptr;
            const std::vector<hex_id_t> *hexes_ = nullptr;

            size_t hex_pos_ = 0;
            RiderHexIndex::rider_set_t::const_iterator rider_it_{};
            RiderHexIndex::rider_set_t::const_iterator rider_end_{};
            bool is_end_ = true;
        };

        Range(const RiderHexIndex *index, const std::vector<hex_id_t> *hexes)
                : index_(index), hexes_(hexes) {}

        [[nodiscard]] iterator begin() const { return {index_, hexes_, false}; }

        [[nodiscard]] iterator end() const { return {index_, hexes_, true}; }

    private:
        const RiderHexIndex *index_;
        const std::vector<hex_id_t> *hexes_;
    };

    [[nodiscard]] Range candidates(coordinate pick_coord) const;

private:
    RiderHexIndex index_;
    int max_radius_;
};
