#pragma once

#include "includes.h"


struct Rider;


class EVCharging {
public:
    EVCharging();

    explicit EVCharging(bool dont_load);

    void update(Rider &rider, distance_t distance) const;

    [[nodiscard]] distance_t start_with_charge() const noexcept;

private:
    distance_t m_max_range{};
    timestamp_t m_charge_time{};

    distance_t m_charge_threshold_km{};
    distance_t m_charge_to_km{};
};
