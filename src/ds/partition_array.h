#pragma once

#include <cstdint>
#include <stdexcept>
#include <vector>
#include <numeric>
#include <span>


template<uint8_t n_partitions, typename T>
    class partition_array {
        static_assert(n_partitions > 0, "Must have at least one partition");

    public:
        using size_type = std::size_t;
        using id_type = std::size_t;

        explicit partition_array(std::vector<T> &&data) noexcept
                : m_data(std::move(data)) {

            m_partition_offsets[0] = 0;
            for (uint8_t i = 1; i <= n_partitions; ++i)
                m_partition_offsets[i] = size();

            m_id_to_index.resize(size());
            m_ids.resize(size());
            std::iota(m_id_to_index.begin(), m_id_to_index.end(), 0);
            std::iota(m_ids.begin(), m_ids.end(), 0);
        }

        partition_array(std::vector<T> &&data, std::vector<size_t> &&ids)
                : m_data(std::move(data)),
                  m_ids(std::move(ids)) {
            if (m_ids.size() != m_data.size())
                throw std::invalid_argument("ids must have the same size as data");

            m_partition_offsets[0] = 0;
            for (uint8_t i = 1; i <= n_partitions; ++i)
                m_partition_offsets[i] = size();

            m_id_to_index.resize(size());
            for (size_t i = 0; i < m_ids.size(); ++i)
                m_id_to_index[m_ids[i]] = i;
        }

        [[nodiscard]] size_t size() const noexcept {
            return m_data.size();
        }

        [[nodiscard]] size_type partition_size(uint8_t p) const {
            check_partition(p);
            return m_partition_offsets[p + 1] - m_partition_offsets[p];
        }

        [[nodiscard]] uint8_t partition_of(size_type id) const {
            auto index = index_of(id);

            for (uint8_t p = 0; p < n_partitions; ++p) {
                if (index < m_partition_offsets[p + 1])
                    return p;
            }

            throw std::logic_error("partition_of: unreachable");
        }

        void move(size_t id, uint8_t to) {
            auto from = partition_of(id);
            auto index = index_of(id) - m_partition_offsets[from];

            _move(from, index, to);
        }

        [[nodiscard]] std::span<T> partitions(uint8_t start, uint8_t n = 1) {
            check_partition(start);
            check_partition(start + n - 1);

            return std::span<T>(
                    m_data.begin() + m_partition_offsets[start],
                    m_data.begin() + m_partition_offsets[start + n]
            );
        }

        [[nodiscard]] std::span<const T> partitions(uint8_t start, uint8_t n = 1) const {
            check_partition(start);
            check_partition(start + n - 1);

            return std::span<const T>(
                    m_data.begin() + m_partition_offsets[start],
                    m_data.begin() + m_partition_offsets[start + n]
            );
        }

        T &operator[](size_t id) {
            return m_data[index_of(id)];
        }

        const T &operator[](size_t id) const {
            return m_data[index_of(id)];
        }

    private:
        std::array<size_type, n_partitions + 1> m_partition_offsets;

        std::vector<T> m_data;

        std::vector<id_type> m_ids;
        std::vector<size_type> m_id_to_index;

        [[nodiscard]] size_t index_of(size_t id) const {
            check_id(id);
            return m_id_to_index[id];
        }

        void check_partition(uint8_t p) const {
            if (p >= n_partitions)
                throw std::out_of_range("Invalid partition index '" + std::to_string(p) + "'");
        }

        void check_id(size_t id) const {
            if (id >= size())
                throw std::out_of_range("Invalid element id");
        }

        void _swap(size_t i, size_t j) {
            std::swap(m_data[i], m_data[j]);

            id_type id_i = m_ids[i];
            id_type id_j = m_ids[j];

            m_ids[i] = id_j;
            m_ids[j] = id_i;

            m_id_to_index[id_i] = j;
            m_id_to_index[id_j] = i;
        }

        void _move(uint8_t from, size_type index, uint8_t to) {
            if (from == to)
                return;
            check_partition(from);
            check_partition(to);

            // only allow moving by swapping across boundary
            size_type from_begin = m_partition_offsets[from];
            size_type from_pos = from_begin + index;

            if (to > from) {
                // moving "forward": swap with boundary elements until it reaches 'to'
                for (uint8_t p = from; p < to; ++p) {
                    size_type boundary = m_partition_offsets[p + 1] - 1;
                    _swap(from_pos, boundary);
                    m_partition_offsets[p + 1]--; // shrink left, expand right
                    from_pos = boundary;
                }
            }
            else {
                // moving "backward"
                for (uint8_t p = from; p > to; --p) {
                    size_type boundary = m_partition_offsets[p];
                    _swap(from_pos, boundary);
                    m_partition_offsets[p]++; // shrink right, expand left
                    from_pos = boundary;
                }
            }
        }
    };
