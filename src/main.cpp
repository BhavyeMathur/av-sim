#define DEBUG false

#include "World.h"
#include "io/Database.h"
#include "io/SimulationConfigs.h"

#include "util/misc.h"

#include <iomanip>
#include <iostream>
#include <thread>
#include <semaphore>
#include <fstream>


static std::string db_path = "runs/runs.sqlite3";


namespace sim {
    thread_local SimulationConfigs configs;
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

    std::vector<std::thread> threads;
    threads.reserve(experiments.size());

    constexpr size_t max_concurrent = 8;
    std::counting_semaphore<max_concurrent> sem(max_concurrent);

    std::cout << "...running " << experiments.size()
              << " experiments with up to " << max_concurrent
              << " concurrent runs\n";

    auto s = std::chrono::high_resolution_clock::now();

    for (const auto &path: experiments) {
        // we allow a maximum of max_concurrent threads
        // and use a semaphore  to guarantee this
        sem.acquire();

        threads.emplace_back([&sem, path]() {
            run(path);
            sem.release();
        });
    }

    for (auto &t: threads)
        t.join();

    auto e = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(e - s);

    std::cout << "Elapsed time: " << duration.count() << " milliseconds\n\n";
    return 0;
}
