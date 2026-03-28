#pragma once

#include "includes.h"
#include "riders/RiderHex.h"
#include "riders/Rider.h"

class AllRidersSource {
public:
    AllRidersSource();

    [[nodiscard]] const std::array<std::vector<rider_id_t>, 1> &candidate_pools(const Request &) const {
        return rider_ids_;
    }

private:
    std::array<std::vector<rider_id_t>, 1> rider_ids_;
};

class HexRidersSource {
public:
    HexRidersSource()
            : index_(sim::riders.size()),
              max_radius_(5) {}

    class Range {
    public:
        class iterator {
        public:
            using iterator_category = std::input_iterator_tag;
            using value_type = const RiderHexIndex::rider_set_t &;
            using difference_type = std::ptrdiff_t;

            iterator(const RiderHexIndex &index, const std::vector<hex_id_t> &hexes, size_t pos = 0)
                    : index_(index),
                      hexes_(hexes),
                      hex_pos_(std::min(pos, hexes_.size())) {}

            const RiderHexIndex::rider_set_t &operator*() const {
                return index_.riders_in_hex(hexes_[hex_pos_]);
            }

            iterator &operator++() {
                if (hex_pos_ < hexes_.size())
                    hex_pos_++;
                return *this;
            }

            bool operator==(const iterator &other) const {
                return &index_ == &other.index_
                       and hexes_ == other.hexes_
                       and hex_pos_ == other.hex_pos_;
            }

            bool operator!=(const iterator &other) const {
                return !(*this == other);
            }

        private:
            const RiderHexIndex &index_;
            const std::vector<hex_id_t> &hexes_;

            size_t hex_pos_ = 0;
        };

        Range(const RiderHexIndex &index, const std::vector<hex_id_t> &hexes)
                : index_(index), hexes_(hexes) {}

        [[nodiscard]] iterator begin() const { return {index_, hexes_}; }

        [[nodiscard]] iterator end() const { return {index_, hexes_, hexes_.size()}; }

    private:
        const RiderHexIndex &index_;
        const std::vector<hex_id_t> &hexes_;
    };

    [[nodiscard]] Range candidate_pools(const Request &req) const;

private:
    RiderHexIndex index_;
    int max_radius_;
};
