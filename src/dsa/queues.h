#pragma once

#include "includes.h"
#include <radix_heap.h>

struct mutable_heap_key {
    uint32_t id = 0;
    uint32_t gen = 0;

    explicit operator bool() const { return id != 0; }
};

template<class T>
    struct mutable_heap_entry {
        mutable_heap_key key;
        T value;

        bool operator<(const mutable_heap_entry &other) const {
            return other.value < value; // min-heap behavior in std::priority_queue
        }
    };

template<class T>
    class mutable_pq {
    public:
        using key = mutable_heap_key;
        using entry = mutable_heap_entry<T>;

        key push(T value) {
            uint32_t id = alloc_id_();
            uint32_t gen = ++gens_[id];

            key k{id, gen};
            q_.push(entry{k, std::move(value)});
            ++live_size_;

            return k;
        }

        void erase(key k) {
            if (!valid_(k)) return;

            ++gens_[k.id];
            free_.push_back(k.id);
            --live_size_;
        }

        T pop() {
            while (!q_.empty()) {
                entry e = std::move(const_cast<entry &>(q_.top()));
                q_.pop();

                if (!valid_(e.key))
                    continue;

                ++gens_[e.key.id];
                free_.push_back(e.key.id);
                --live_size_;

                return std::move(e.value);
            }

            throw std::out_of_range("called pop() on empty mutable_pq");
        }

        [[nodiscard]] bool empty() const {
            return live_size_ == 0;
        }

        [[nodiscard]] size_t size() const {
            return live_size_;
        }

    private:
        uint32_t alloc_id_() {
            if (!free_.empty()) {
                uint32_t id = free_.back();
                free_.pop_back();
                return id;
            }

            uint32_t id = static_cast<uint32_t>(gens_.size());
            gens_.push_back(0);
            return id;
        }

        bool valid_(key k) const {
            return k && k.id < gens_.size() && gens_[k.id] == k.gen;
        }

        std::priority_queue<entry> q_;

        std::vector<uint32_t> gens_{0};
        std::vector<uint32_t> free_;
        size_t live_size_ = 0;
    };

template<class T>
    struct default_radix_key {
        using key_type = T;

        key_type operator()(const T &value) const {
            return value;
        }
    };

template<class T,
         class KeyOf = default_radix_key<T>,
         class RadixKey = typename KeyOf::key_type>
    class mutable_radix_heap {
    public:
        using key = mutable_heap_key;
        using entry = mutable_heap_entry<T>;

        key push(T value) {
            uint32_t id = alloc_id_();
            uint32_t gen = ++gens_[id];

            key k{id, gen};

            RadixKey rk = get_key_(value);
            q_.push(rk, entry{k, std::move(value)});

            ++live_size_;
            return k;
        }

        void erase(key k) {
            if (!valid_(k)) return;

            ++gens_[k.id];
            free_.push_back(k.id);
            --live_size_;
        }

        T pop() {
            while (!q_.empty()) {
                entry e = std::move(q_.top_value());
                q_.pop();

                if (!valid_(e.key))
                    continue;

                ++gens_[e.key.id];
                free_.push_back(e.key.id);
                --live_size_;

                return std::move(e.value);
            }

            throw std::out_of_range("called pop() on empty mutable_radix_heap");
        }

        [[nodiscard]] bool empty() const {
            return live_size_ == 0;
        }

        [[nodiscard]] size_t size() const {
            return live_size_;
        }

    private:
        uint32_t alloc_id_() {
            if (!free_.empty()) {
                uint32_t id = free_.back();
                free_.pop_back();
                return id;
            }

            uint32_t id = static_cast<uint32_t>(gens_.size());
            gens_.push_back(0);
            return id;
        }

        bool valid_(key k) const {
            return k && k.id < gens_.size() && gens_[k.id] == k.gen;
        }

        radix_heap<RadixKey, entry> q_;

        std::vector<uint32_t> gens_{0}; // slot 0 unused
        std::vector<uint32_t> free_;
        size_t live_size_ = 0;

        KeyOf get_key_{};
    };
