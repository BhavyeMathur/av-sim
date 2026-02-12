#pragma once

#include "util/event_vector.h"

class Speed {
public:
    explicit Speed(const std::string &speed_config_file);

    void update();

    [[nodiscard]] speed_t lm_speed_kmps() const;

    [[nodiscard]] speed_t fm_speed_kmps() const;

    [[nodiscard]] speed_t ambient_speed_kmps() const;

private:
    struct SpeedEvent {
        timestamp_t at;
        speed_t avg_fm_speed;
        speed_t avg_lm_speed;
    };

    speed_t m_lm_speed_kmps = 0;
    speed_t m_fm_speed_kmps = 0;

    event_vector<SpeedEvent, &SpeedEvent::at> m_events;
};
