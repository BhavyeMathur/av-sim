#pragma once

#include <string>

struct SimulationConfigs {
    struct Sim {
        uint32_t length_s = 0;
        int h3_resolution = 0;

        std::string requests_file;
        std::string riders_file;
        std::string output;
    };

    struct Policy {
        std::string matching;

        std::string charging;
        std::string charging_hubs;
    };

    struct Fleet {
        double frac_2_seater = 0.0;
        double frac_4_seater = 0.0;
        double frac_6_seater = 0.0;
        int fleet_size = 0;
    };

    struct Stats {
        uint32_t fleet_log_interval = 900;
    };

    std::string name;

    Sim sim;
    Policy policy;
    Fleet fleet;
    Stats stats;
};

SimulationConfigs load_config(const std::string &yaml_path);
