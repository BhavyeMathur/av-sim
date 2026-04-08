#include "SimulationConfigs.h"

#include <stdexcept>
#include <yaml-cpp/yaml.h>

template<typename T>
    T require_scalar(const YAML::Node &parent, const std::string &key) {
        if (!parent[key])
            throw std::runtime_error("Missing required key: " + key);

        try {
            return parent[key].as<T>();
        } catch (const std::exception &e) {
            throw std::runtime_error("Invalid type for key '" + key + "': " + std::string(e.what()));
        }
    }

template<typename T>
    T optional_scalar(const YAML::Node &parent, const std::string &key, T default_value) {
        if (!parent[key])
            return default_value;

        try {
            return parent[key].as<T>();
        } catch (const std::exception &e) {
            throw std::runtime_error("Invalid type for key '" + key + "': " + std::string(e.what()));
        }
    }

void validate_config(const SimulationConfigs &cfg) {
    if (cfg.sim.length_s <= 0)
        throw std::runtime_error("sim.length_s must be positive");

    if (cfg.sim.h3_resolution < 0 or cfg.sim.h3_resolution > 15)
        throw std::runtime_error("sim.h3_resolution must be in [0, 15]");

    if (cfg.fleet.fleet_size <= 0)
        throw std::runtime_error("fleet.fleet_size must be positive");

    if (cfg.fleet.frac_2_seater < 0.0 or cfg.fleet.frac_2_seater > 1.0)
        throw std::runtime_error("fleet.frac_2_seater must be in [0, 1]");

    if (cfg.fleet.frac_4_seater < 0.0 or cfg.fleet.frac_4_seater > 1.0)
        throw std::runtime_error("fleet.frac_4_seater must be in [0, 1]");

    if (cfg.fleet.frac_2_seater + cfg.fleet.frac_4_seater > 1.0)
        throw std::runtime_error("fleet.frac_2_seater + fleet.frac_4_seater must be <= 1");

    if (cfg.sim.requests_file.empty())
        throw std::runtime_error("sim.requests_file cannot be empty");

    if (cfg.sim.riders_file.empty())
        throw std::runtime_error("sim.riders_file cannot be empty");

    if (cfg.sim.output.empty())
        throw std::runtime_error("sim.output cannot be empty");
}

SimulationConfigs load_config(const std::string &yaml_path) {
    YAML::Node root;
    try {
        root = YAML::LoadFile(yaml_path);
    } catch (const std::exception &e) {
        throw std::runtime_error("Failed to load YAML file '" + yaml_path + "': " + e.what());
    }

    SimulationConfigs cfg;

    cfg.name = require_scalar<std::string>(root, "name");

    if (!root["sim"])
        throw std::runtime_error("Missing required section: sim");
    if (!root["policy"])
        throw std::runtime_error("Missing required section: policy");
    if (!root["fleet"])
        throw std::runtime_error("Missing required section: fleet");

    const auto sim_node = root["sim"];
    const auto policy_node = root["policy"];
    const auto fleet_node = root["fleet"];

    cfg.sim.length_s = require_scalar<int>(sim_node, "length_s");
    cfg.sim.requests_file = require_scalar<std::string>(sim_node, "requests_file");
    cfg.sim.h3_resolution = optional_scalar<int>(sim_node, "h3_resolution", 7);
    cfg.sim.riders_file = require_scalar<std::string>(sim_node, "riders_file");
    cfg.sim.output = require_scalar<std::string>(sim_node, "output");

    cfg.policy.matching = require_scalar<std::string>(policy_node, "matching");
    cfg.policy.charging = optional_scalar<std::string>(policy_node, "charging", "disable");
    cfg.policy.charging_hubs = optional_scalar<std::string>(policy_node, "charging_hubs", "");

    cfg.fleet.frac_2_seater = require_scalar<double>(fleet_node, "frac_2_seater");
    cfg.fleet.frac_4_seater = require_scalar<double>(fleet_node, "frac_4_seater");
    cfg.fleet.fleet_size = require_scalar<int>(fleet_node, "fleet_size");

    cfg.fleet.frac_6_seater = 1 - cfg.fleet.frac_2_seater - cfg.fleet.frac_4_seater;

    validate_config(cfg);
    return cfg;
}
