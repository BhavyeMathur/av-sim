#pragma once

#include "includes.h"
#include "ds/event_vector.h"

#include <fstream>

struct ConfigKey {
    zone_id_t zone_id;
    customer_id_t customer_id;

    bool operator==(const ConfigKey &other) const {
        return zone_id == other.zone_id && customer_id == other.customer_id;
    }
};

inline std::istream &operator>>(std::istream &is, ConfigKey &x) {
    return is >> x.zone_id >> x.customer_id;
}

inline std::ostream &operator<<(std::ostream &os, ConfigKey &x) {
    os << "zone=";
    if (x.zone_id == AnyZone)
        os << "N/A";
    else
        os << x.zone_id;

    os << ", customer=";
    if (x.customer_id == AnyCustomer)
        os << "N/A";
    else
        os << x.customer_id;
    return os;
}

struct ConfigKeyHash {
    size_t operator()(const ConfigKey &key) const noexcept {
        size_t h = 0;
        std::memcpy(&h, &key, sizeof(ConfigKey));
        return std::hash<size_t>{}(h);
    }
};

template<typename ConfigT>
    class AllocationEngineConfig {
    public:
        explicit AllocationEngineConfig(const std::string &filename)
                : m_events([&]() {
            std::vector<ConfigEvent> events;

            std::ifstream file(filename);
            if (!file.is_open())
                throw std::invalid_argument("AllocationEngine could not open config file '" + filename + "'");
            file.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // ignore header

            timestamp_t timestamp;
            ConfigKey key{};
            ConfigT value{};
            bool kill;

            while (file >> timestamp >> key >> kill) {
                if (kill)
                    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                else
                    file >> value;
                events.push_back({timestamp, key, value, kill});
            }

            //                    cout << "Loaded " << events.size() << " config events" << endl;

            file.close();
            return events;
        }()) {

            update();
            assert(m_config.contains({AnyZone, AnyCustomer}));
            cout << "\t" << m_config.at({AnyZone, AnyCustomer}) << endl;
        }

        void update() {
            while (auto event = m_events.check()) {
                if (event->kill) {
                    //                     cout << "config killed @ " << clock << " " << event->key << endl;
                    m_config.erase(event->key);
                    assert(m_config.contains({AnyZone, AnyCustomer}));
                }

                else {
                    //                     cout << "config updated @ " << clock << " " << event->key << " to " << m_config[event->key] << endl;
                    m_config[event->key] = event->value;
                }
            }
        }

        [[nodiscard]] ConfigT get(zone_id_t zone_id, customer_id_t customer_id) const {
            const ConfigKey cands[4] = {
                    {zone_id, customer_id},
                    {zone_id, AnyCustomer},
                    {AnyZone, customer_id},
            };

            for (auto &c: cands)
                if (auto it = m_config.find(c); it != m_config.end())
                    return it->second;

            return m_config.at({AnyZone, AnyCustomer});
        }

    private:
        struct ConfigEvent {
            timestamp_t at;
            ConfigKey key;
            ConfigT value;
            bool kill;
        };

        event_vector<ConfigEvent, &ConfigEvent::at> m_events;
        std::unordered_map<ConfigKey, ConfigT, ConfigKeyHash> m_config;
    };


struct FirstMileConfig {
    distance_t min_fm_km;
    distance_t max_fm_km;
};

struct LastMileConfig {
    distance_t min_lm_km;
    distance_t max_lm_km;
};

struct SLAConfig {
    duration_t max_kwt_s;
    duration_t sla_deg_s;
    duration_t drop_time_s;
    speed_t speed_kmps;
};

struct RPHConfig {
    float min_time_s;
    float max_time_s;
};

inline std::istream &operator>>(std::istream &is, FirstMileConfig &x) {
    return is >> x.min_fm_km >> x.max_fm_km;
}

inline std::istream &operator>>(std::istream &is, LastMileConfig &x) {
    return is >> x.min_lm_km >> x.max_lm_km;
}

inline std::istream &operator>>(std::istream &is, SLAConfig &x) {
    return is >> x.max_kwt_s >> x.sla_deg_s >> x.drop_time_s >> x.speed_kmps;
}

inline std::istream &operator>>(std::istream &is, RPHConfig &x) {
    return is >> x.min_time_s >> x.max_time_s;
}

inline std::ostream &operator<<(std::ostream &os, FirstMileConfig &x) {
    return os << "min_fm=" << x.min_fm_km << ", max_fm=" << x.max_fm_km;
}

inline std::ostream &operator<<(std::ostream &os, LastMileConfig &x) {
    return os << "min_lm=" << x.min_lm_km << ", max_lm=" << x.max_lm_km;
}

inline std::ostream &operator<<(std::ostream &os, SLAConfig &x) {
    return os << "max_kwt=" << x.max_kwt_s << ", sla_deg=" << x.sla_deg_s << ", drop_time=" << x.drop_time_s
              << ", speed=" << x.speed_kmps;
}

inline std::ostream &operator<<(std::ostream &os, RPHConfig &x) {
    return os << "min_time=" << x.min_time_s << ", max_time=" << x.max_time_s;
}
