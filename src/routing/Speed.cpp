#include "Speed.h"

#include <fstream>

Speed::Speed(const std::string &speed_config_file)
        : m_events(
        [&]() {
            std::vector<SpeedEvent> events;

            std::ifstream file(speed_config_file);
            if (!file.is_open())
                throw std::invalid_argument("could not open speed config file '" + speed_config_file + "'");
            file.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // ignore header

            timestamp_t timestamp;
            speed_t fm_speed;
            speed_t lm_speed;

            while (file >> timestamp >> fm_speed >> lm_speed)
                events.push_back({timestamp, fm_speed, lm_speed});

            file.close();
            return events;
        }()) {

    m_fm_speed_kmps = m_events.peek()->avg_fm_speed;
    m_lm_speed_kmps = m_events.peek()->avg_lm_speed;
}

void Speed::update() {
    if (auto event = m_events.check()) {
        m_fm_speed_kmps = event->avg_fm_speed;
        m_lm_speed_kmps = event->avg_lm_speed;
    }
}

speed_t Speed::fm_speed_kmps() const {
    return m_fm_speed_kmps;
}

speed_t Speed::lm_speed_kmps() const {
    return m_lm_speed_kmps;
}

speed_t Speed::ambient_speed_kmps() const {
    return (fm_speed_kmps() + lm_speed_kmps()) / 2;
}
