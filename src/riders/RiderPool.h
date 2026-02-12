#pragma once

#include "includes.h"
#include <partition_array.h>


struct RidersDataFrame;
struct Rider;

class RiderPool {
public:
    explicit RiderPool(std::vector<Rider> &&riders);

    explicit RiderPool(const RidersDataFrame &riders);

    [[nodiscard]] size_t size() const noexcept;

    void spawn(rider_id_t id);

    void update();

    void update(const Rider &rider);

    void kill(rider_id_t id);

    [[nodiscard]] std::span<const Rider> available_riders() const noexcept;

    [[nodiscard]] std::span<const Rider> all_riders() const noexcept;

    [[nodiscard]] std::span<Rider> alive_riders() noexcept;

    [[nodiscard]] std::span<Rider> idle_riders() noexcept;

    const Rider &operator[](rider_id_t id) const;

    Rider &operator[](rider_id_t id);

private:
    static constexpr uint8_t dead_partition = 0;
    static constexpr uint8_t busy_partition = 1;
    static constexpr uint8_t jit_partition = 2;
    static constexpr uint8_t idle_partition = 3;

    partition_array<4, Rider> m_riders;

    void _print_debug() const;
};
