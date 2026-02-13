#pragma once

#include "includes.h"


template<class T>
    class mutable_pq {
    public:
        struct key {
            uint32_t id = 0;   // 0 = invalid
            uint32_t gen = 0;

            explicit operator bool() const { return id != 0; }
        };

        mutable_pq() {
            records_.resize(1); // slot 0 unused
        }

        void reserve(size_t n) {
            records_.reserve(n + 1);
            freelist_.reserve(n / 2 + 1); // heuristic
            q_.reserve(n);
        }

        key push(T value) {
            const uint32_t id = alloc_slot_();
            record &r = records_[id];

            r.gen += 1;
            r.live = true;
            r.value = std::move(value);

            key k{id, r.gen};
            q_.push(k);
            return k;
        }

        void erase(key k) {
            if (!k or k.id >= records_.size())
                return;

            record &r = records_[k.id];
            if (!r.live or r.gen != k.gen)
                return;

            r.live = false;
            freelist_.push_back(k.id);
        }

        T pop() {
            while (!q_.empty()) {
                key k = q_.pop();

                if (!k or k.id >= records_.size())
                    continue;
                record &r = records_[k.id];

                if (!r.live or r.gen != k.gen)
                    continue;

                r.live = false;
                freelist_.push_back(k.id);
                return std::move(r.value);
            }
            throw std::out_of_range("called pop() on empty object");
        }

        [[nodiscard]] bool empty() const { return q_.empty(); }

    private:
        struct record {
            T value{};
            uint32_t gen = 0;
            bool live = false;
        };

        uint32_t alloc_slot_() {
            if (!freelist_.empty()) {
                uint32_t id = freelist_.back();
                freelist_.pop_back();
                return id;
            }
            auto id = static_cast<uint32_t>(records_.size());
            records_.push_back(record{});
            return id;
        }

        struct key_comparator {
            const std::vector<record> *recs;

            bool operator()(key a, key b) const {
                return (*recs)[a.id].value < (*recs)[b.id].value;
            }
        };

        std::priority_queue<key, std::vector<key>, key_comparator> q_;
        std::vector<record> records_;
        std::vector<uint32_t> freelist_;
    };
