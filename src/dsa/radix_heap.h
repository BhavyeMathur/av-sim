#pragma once

// based on https://github.com/iwiwi/radix-heap

#include "includes.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace internal {
    template<bool Is64bit>
        class find_bucket_impl;

    template<>
        class find_bucket_impl<false> {
        public:
            static inline constexpr size_t find_bucket(uint32_t x, uint32_t last) {
                return x == last ? 0 : 32 - __builtin_clz(x ^ last);
            }
        };

    template<>
        class find_bucket_impl<true> {
        public:
            static inline constexpr size_t find_bucket(uint64_t x, uint64_t last) {
                return x == last ? 0 : 64 - __builtin_clzll(x ^ last);
            }
        };

    template<class T>
        inline constexpr size_t find_bucket(T x, T last) {
            return find_bucket_impl<sizeof(T) == 8>::find_bucket(x, last);
        }

    template<class KeyType, bool IsSigned>
        class encoder_impl_integer;

    template<class KeyType>
        class encoder_impl_integer<KeyType, false> {
        public:
            using key_type = KeyType;
            using unsigned_key_type = KeyType;

            inline static constexpr unsigned_key_type encode(key_type x) {
                return x;
            }

            inline static constexpr key_type decode(unsigned_key_type x) {
                return x;
            }
        };

    template<class KeyType>
        class encoder_impl_integer<KeyType, true> {
        public:
            using key_type = KeyType;
            using unsigned_key_type = typename std::make_unsigned<KeyType>::type;

            inline static constexpr unsigned_key_type encode(key_type x) {
                return static_cast<unsigned_key_type>(x) ^
                       (unsigned_key_type(1) << unsigned_key_type(
                               std::numeric_limits<unsigned_key_type>::digits - 1));
            }

            inline static constexpr key_type decode(unsigned_key_type x) {
                return static_cast<key_type>(
                        x ^ (unsigned_key_type(1) <<
                                                  (std::numeric_limits<unsigned_key_type>::digits - 1)));
            }
        };

    template<class KeyType>
        class encoder : public encoder_impl_integer<KeyType, std::is_signed<KeyType>::value> {};
}

template<class KeyType,
         class ValueType,
         class Compare = std::less<ValueType>,
         class EncoderType = internal::encoder<KeyType>>
    class radix_heap {
    public:
        using key_type = KeyType;
        using value_type = ValueType;
        using compare_type = Compare;
        using encoder_type = EncoderType;
        using unsigned_key_type = typename encoder_type::unsigned_key_type;

    private:
        using entry_type = std::pair<unsigned_key_type, value_type>;

    public:
        radix_heap()
                : size_(0), last_(), compare_(), buckets_() {
            buckets_min_.fill(std::numeric_limits<unsigned_key_type>::max());
        }

        explicit radix_heap(compare_type compare)
                : size_(0), last_(), compare_(std::move(compare)), buckets_() {
            buckets_min_.fill(std::numeric_limits<unsigned_key_type>::max());
        }

        void reserve(size_t n) {
            for (auto &b: buckets_)
                b.reserve(n / buckets_.size() + 1);
        }

        void push(key_type key, const value_type &value) {
            const unsigned_key_type x = encoder_type::encode(key);
            assert(last_ <= x);

            ++size_;
            const size_t k = internal::find_bucket(x, last_);

            buckets_[k].emplace_back(x, value);
            buckets_min_[k] = std::min(buckets_min_[k], x);
        }

        void push(key_type key, value_type &&value) {
            const unsigned_key_type x = encoder_type::encode(key);
            assert(last_ <= x);

            ++size_;
            const size_t k = internal::find_bucket(x, last_);

            buckets_[k].emplace_back(x, std::move(value));
            buckets_min_[k] = std::min(buckets_min_[k], x);
        }

        template<class... Args>
            void emplace(key_type key, Args &&... args) {
                const unsigned_key_type x = encoder_type::encode(key);
                assert(last_ <= x);

                ++size_;
                const size_t k = internal::find_bucket(x, last_);

                buckets_[k].emplace_back(
                        std::piecewise_construct,
                        std::forward_as_tuple(x),
                        std::forward_as_tuple(std::forward<Args>(args)...));

                buckets_min_[k] = std::min(buckets_min_[k], x);
            }

        key_type top_key() {
            pull();
            return encoder_type::decode(last_);
        }

        value_type &top_value() {
            pull();
            return *best_;
        }

        const value_type &top_value() const {
            const_cast<radix_heap *>(this)->pull();
            return *best_;
        }

        value_type pop() {
            pull();

            auto &b = buckets_[0];
            value_type value = std::move(*best_);

            if (best_i_ + 1 != b.size())
                b[best_i_] = std::move(b.back());

            b.pop_back();
            --size_;
            best_ = nullptr;

            return value;
        }

        size_t size() const {
            return size_;
        }

        bool empty() const {
            return size_ == 0;
        }

        void clear() {
            size_ = 0;
            last_ = unsigned_key_type();

            for (auto &b: buckets_)
                b.clear();

            buckets_min_.fill(std::numeric_limits<unsigned_key_type>::max());
            best_ = nullptr;
            best_i_ = 0;
        }

        void swap(radix_heap &a) {
            std::swap(size_, a.size_);
            std::swap(last_, a.last_);
            std::swap(compare_, a.compare_);
            buckets_.swap(a.buckets_);
            buckets_min_.swap(a.buckets_min_);
            std::swap(best_, a.best_);
            std::swap(best_i_, a.best_i_);
        }

    private:
        size_t size_;
        unsigned_key_type last_;
        compare_type compare_;

        std::array<std::vector<entry_type>,
                std::numeric_limits<unsigned_key_type>::digits + 1> buckets_;

        std::array<unsigned_key_type,
                std::numeric_limits<unsigned_key_type>::digits + 1> buckets_min_;

        value_type *best_ = nullptr;
        size_t best_i_ = 0;

        void pull() {
            assert(size_ > 0);

            if (buckets_[0].empty()) {
                size_t i;
                for (i = 1; buckets_[i].empty(); ++i);

                last_ = buckets_min_[i];

                for (size_t j = 0; j < buckets_[i].size(); ++j) {
                    const unsigned_key_type x = buckets_[i][j].first;
                    const size_t k = internal::find_bucket(x, last_);

                    buckets_[k].emplace_back(std::move(buckets_[i][j]));
                    buckets_min_[k] = std::min(buckets_min_[k], x);
                }

                buckets_[i].clear();
                buckets_min_[i] = std::numeric_limits<unsigned_key_type>::max();
            }

            find_best_();
        }

        void find_best_() {
            auto &b = buckets_[0];

            best_i_ = 0;
            best_ = &b[0].second;

            for (size_t i = 1; i < b.size(); ++i) {
                auto &value = b[i].second;

                if (compare_(value, *best_)) {
                    best_i_ = i;
                    best_ = &value;
                }
            }
        }
    };
