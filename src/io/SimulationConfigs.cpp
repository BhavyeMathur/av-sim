#include "SimulationConfigs.h"

#include <fstream>
#include <stdexcept>


SimulationConfigs::SimulationConfigs(const std::string &configs) {
    std::ifstream file(configs);
    if (!file.is_open())
        throw std::invalid_argument("Could not open file '" + configs + "'");

    std::string key;
    std::string value;
    while (file >> key >> value) {
        key.pop_back();  // remove ':'
        m_configs.insert({key, value});
    }
}
