#include "RiderPool.h"
#include "Rider.h"
#include "io/RidersDataframe.h"

#include <algorithm>


using namespace std;

RiderPool::RiderPool(std::vector<Rider> &&riders)
        : m_riders(
        [&]() {
            sort(riders.begin(), riders.end(), [](const Rider &lhs, const Rider &rhs) {
                return lhs.eta_at < rhs.eta_at;
            });

            std::vector<size_t> rider_ids;
            rider_ids.reserve(riders.size());

            for (const auto &rider: riders)
                rider_ids.emplace_back(rider.id);

            return partition_array<4, Rider>(std::move(riders),
                                             std::move(rider_ids));
        }()) {
}

RiderPool::RiderPool(const RidersDataFrame &riders)
        : RiderPool(
        [&riders]() {
            std::vector<Rider> rider_data;
            rider_data.reserve(riders.size());

            for (const auto &rider: riders) {
                coordinate coord(static_cast<coordinate_t>(rider.lat),
                                 static_cast<coordinate_t>(rider.lon));
                rider_data.emplace_back(static_cast<rider_id_t>(rider.id),
                                        coord,
                                        static_cast<timestamp_t>(rider.created_at), // login_at
                                        static_cast<timestamp_t>(rider.created_at + rider.lifetime) // logout_at
                );
            }

            return rider_data;
        }()) {
}

size_t RiderPool::size() const noexcept {
    return m_riders.size();
}

void RiderPool::spawn(rider_id_t id) {
    m_riders[id].spawn();
    m_riders.move(id, idle_partition);
    _print_debug();
}

void RiderPool::update() {
    for (auto &rider: alive_riders())
        if (rider.update())
            update(rider);
}

void RiderPool::update(const Rider &rider) {
    assert(rider.is_alive());

    if (rider.state == Rider::State::IDLE)
        m_riders.move(rider.id, idle_partition);

    else if (rider.n_active_orders > 1 ||
             rider.state == Rider::State::FM ||
             rider.state == Rider::State::WAIT ||
             rider.state == Rider::State::UNAVAILABLE)
        m_riders.move(rider.id, busy_partition);

    else
        m_riders.move(rider.id, jit_partition);
}

void RiderPool::kill(rider_id_t id) {
    assert(m_riders.partition_of(id) != dead_partition);

    m_riders[id].kill();
    m_riders.move(id, dead_partition);
    _print_debug();
}

std::span<const Rider> RiderPool::available_riders() const noexcept {
    return m_riders.partitions(jit_partition, 2);
}

std::span<const Rider> RiderPool::all_riders() const noexcept {
    return m_riders.partitions(0, 4);
}

std::span<Rider> RiderPool::alive_riders() noexcept {
    return m_riders.partitions(1, 3);
}

std::span<Rider> RiderPool::idle_riders() noexcept {
    return m_riders.partitions(idle_partition, 1);
}

Rider &RiderPool::operator[](rider_id_t id) {
    return m_riders[id];
}

const Rider &RiderPool::operator[](rider_id_t id) const {
    return m_riders[id];
}

void RiderPool::_print_debug() const {
    #ifdef DEBUG
    for (auto &rider: free_riders())
        std::cout << rider.id << '\n';

    std::cout << "----\n";

    for (auto &rider: busy_riders())
        std::cout << rider.id << '\n';

    std::cout << '\n';
    #endif
}
