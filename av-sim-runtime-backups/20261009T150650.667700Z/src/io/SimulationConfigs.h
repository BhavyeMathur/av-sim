#pragma once

#include <string>

struct SimulationConfigs {
    struct Sim {
        uint32_t length_s = 0;
        int h3_resolution = 0;

        std::string requests_file;
        std::string riders_file;
        std::string output;

        float cos_ref_lat;
    };

    struct Policy {
        std::string matching;

        std::string charging;
        std::string charging_hubs;

        uint32_t max_response_time;
    };

    struct Fleet {
        double frac_2_seater = 0.0;
        double frac_4_seater = 0.0;
        double frac_6_seater = 0.0;
        int fleet_size = 0;

        float max_range = 0.8 * 300;    // 240 km
        uint32_t charge_time = 3600;    // 1 hour
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
