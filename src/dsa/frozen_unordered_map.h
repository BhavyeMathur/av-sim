#pragma once

#include <vector>
#include <algorithm>
#include <initializer_list>
#include <utility>
#include <optional>


template<typename K, typename V>
    class frozen_unordered_map {
    public:
        using value_type = std::pair<K, V>;
        using container_type = std::vector<value_type>;
        using const_iterator = typename container_type::const_iterator;

        frozen_unordered_map() = default;

        frozen_unordered_map(std::initializer_list<value_type> init) {
            m_data.reserve(init.size());
            for (auto &&kv: init)
                m_data.emplace_back(std::move(kv));
            finalize();
        }

        explicit frozen_unordered_map(std::vector<value_type> &&init)
                : m_data(init) {
            finalize();
        }

        template<typename It>
            frozen_unordered_map(It first, It last) {
                m_data.assign(first, last);
                finalize();
            }

        [[nodiscard]] const_iterator find(const K &key) const {
            auto it = std::lower_bound(m_data.begin(), m_data.end(), key,
                                       [](auto const &kv, const K &k) {
                                           return kv.first < k;
                                       });
            if (it == m_data.end() || it->first == key)
                return it;
            return m_data.end();
        }

        [[nodiscard]] bool contains(const K &key) const {
            return find(key) != m_data.end();
        }

        [[nodiscard]] const V &at(const K &key) const {
            auto it = find(key);
            if (it == m_data.end())
                throw std::out_of_range("const_unordered_map::at: key not found");
            return it->second;
        }

        [[nodiscard]] const_iterator begin() const noexcept {
            return m_data.begin();
        }

        [[nodiscard]] const_iterator end() const noexcept {
            return m_data.end();
        }

        [[nodiscard]] size_t size() const noexcept {
            return m_data.size();
        }

        [[nodiscard]] bool empty() const noexcept {
            return m_data.empty();
        }

    private:
        container_type m_data;

        void finalize() {
            std::sort(m_data.begin(), m_data.end(),
                      [](auto const &a, auto const &b) {
                          return a.first < b.first;
                      });
        }
    };
