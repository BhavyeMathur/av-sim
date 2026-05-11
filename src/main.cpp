#define DEBUG false

#include "World.h"
#include "io/Database.h"
#include "io/SimulationConfigs.h"
#include "events/EventBus.h"

#include "util/misc.h"

#include <iomanip>
#include <iostream>
#include <fstream>

static std::string db_path = "runs/runs.sqlite3";

mutable_radix_heap<Event, EventBus::_event_radix_key> EventBus::events_;

namespace sim {
    SimulationConfigs configs;
}

void run(const std::string &config) {
    sim::configs = load_config(config);

    const std::string started_at = utc_now_iso8601();
    auto s = std::chrono::high_resolution_clock::now();

    create_world();

    auto e = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(e - s);

    Database db(db_path);
    db.create_run(sim::configs, config, started_at, duration.count());
}

std::vector<std::string> read_manifest(const std::string &manifest_path) {
    std::ifstream in(manifest_path);
    if (!in)
        throw std::runtime_error("Failed to open manifest file: " + manifest_path);

    std::vector<std::string> experiments;
    std::string line;

    while (std::getline(in, line))
        if (!line.empty() and line[0] != '#')
            experiments.push_back(line + "config.yaml");

    return experiments;
}

int main(int argc, char *argv[]) {
    std::cout << std::setprecision(4) << std::fixed;

    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " manifest.txt\n";
        return 1;
    }

    auto experiments = read_manifest(argv[1]);
    if (experiments.empty()) {
        std::cerr << "Manifest contains no experiment files: " << argv[1] << '\n';
        return 1;
    }

    // initialize database and schema
    {
        Database db(db_path);
        db.init_schema();
    }

    run(experiments[0]);
    return 0;
}
