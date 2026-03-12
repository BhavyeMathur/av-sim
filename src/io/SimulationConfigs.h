#pragma once

#include <string>
#include <sstream>
#include <unordered_map>


class SimulationConfigs {
public:
    SimulationConfigs() = default;

    explicit SimulationConfigs(const std::string &config_file);

    template<typename T>
        T get(const std::string &key) const {
            auto it = m_configs.find(key);
            if (it == m_configs.end())
                throw std::out_of_range("SimulationConfigs: key not found: " + key);

            const std::string &raw = it->second;

            std::istringstream iss(raw);
            T value;

            if (!(iss >> value))
                throw std::runtime_error("SimulationConfigs: failed to parse key '" + key +
                                         "' with value '" + raw + "' into requested type.");

            std::string leftover;
            if (iss >> leftover)
                throw std::runtime_error("SimulationConfigs: extra trailing characters for key '" +
                                         key + "': '" + raw + "'");

            return value;
        }

    template<typename T>
        T get(const std::string &key, T default_) const {
            try { return get<T>(key); }
            catch (std::out_of_range &) {}

            return default_;
        }

private:
    std::unordered_map<std::string, std::string> m_configs;
};
