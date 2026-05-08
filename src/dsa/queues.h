#pragma once

#include "includes.h"
#include <radix_heap.h>

struct _mutable_heap_key {
    uint32_t id = 0;   // 0 = invalid
    uint32_t gen = 0;

    explicit operator bool() const { return id != 0; }
};

template<class T>
    struct _mutable_heap_record {
        T value{};
        uint32_t gen = 0;
        bool live = false;
    };

template<class T>
    struct _key_comparator {
        using key = _mutable_heap_key;
        using record = _mutable_heap_record<T>;

        const std::vector<record> *records_ = nullptr;

        bool operator()(key a, key b) const {
            const auto &ra = (*records_)[a.id].value;
            const auto &rb = (*records_)[b.id].value;
            return rb < ra;
        }
    };

template<class T, class heap_t>
    class mutable_heap {
    public:
        using key = _mutable_heap_key;
        using record = _mutable_heap_record<T>;
        using comp = _key_comparator<T>;

        mutable_heap() {
            q_ = heap_t(comp{&this->records_});
            records_.resize(1); // slot 0 unused
        }

        mutable_heap(const mutable_heap &) = delete;

        mutable_heap &operator=(const mutable_heap &) = delete;

        mutable_heap(mutable_heap &&other) noexcept = delete;

        mutable_heap &operator=(mutable_heap &&other) noexcept = delete;

        void reserve(size_t n) {
            records_.reserve(n + 1);
            freelist_.reserve(n / 2 + 1); // heuristic
            if constexpr (requires { q_.reserve(n); })
                q_.reserve(n);
        }

        key push(T value) {
            const uint32_t id = alloc_slot_();
            record &r = records_[id];

            r.gen += 1;
            r.live = true;
            r.value = std::move(value);

            key k{id, r.gen};
            push_impl(k, r.value);
            this->live_size_++;

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
            live_size_--;
        }

        T pop() {
            while (!q_.empty()) {
                key k;
                if constexpr (requires { q_.top_value(); })
                    k = q_.top_value();
                else
                    k = q_.top();
                q_.pop();

                if (!k or k.id >= records_.size())
                    continue;
                record &r = records_[k.id];

                if (!r.live or r.gen != k.gen)
                    continue;

                r.live = false;
                freelist_.push_back(k.id);
                live_size_--;
                return std::move(r.value);
            }

            throw std::out_of_range("called pop() on empty object");
        }

        [[nodiscard]] bool empty() const { return live_size_ == 0; }

        [[nodiscard]] size_t size() const { return live_size_; }

    protected:
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

        virtual void push_impl(const key &key, const T &value) = 0;

        heap_t q_;
        std::vector<record> records_;
        std::vector<uint32_t> freelist_;
        size_t live_size_ = 0;
    };

template<class T>
    class mutable_pq : public mutable_heap<T,
            std::priority_queue<_mutable_heap_key, std::vector<_mutable_heap_key>, _key_comparator<T>>> {
    public:
        using key = _mutable_heap_key;

    protected:
        void push_impl(const key &key, const T &value) override {
            this->q_.push(key);
        }
    };

template<class T>
    struct _default_radix_key {
        using key_type = T;

        key_type operator()(const T &value) const {
            return value;
        }
    };

template<class T,
         class KeyOf = _default_radix_key<T>,
         class RadixKey = typename KeyOf::key_type>
    class mutable_radix_heap final
            : public mutable_heap<T, radix_heap<RadixKey, _mutable_heap_key, _key_comparator<T>>> {
    public:
        using key = _mutable_heap_key;

    protected:
        void push_impl(const key &key, const T &value) override {
            this->q_.push(get_key_(value), key);
        }

    private:
        KeyOf get_key_{};
    };
