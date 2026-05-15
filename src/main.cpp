#define DEBUG false

#include "World.h"
#include "io/Database.h"
#include "io/SimulationConfigs.h"

#include "util/misc.h"

#include <iomanip>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>

static std::string db_path = "runs/runs.sqlite3";

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

    constexpr size_t MAX_PROCS = 8;

    std::vector<pid_t> pids;
    size_t next = 0;
    size_t alive = 0;

    auto wait_one = [&]() {
        int status = 0;
        pid_t pid = wait(&status);

        if (pid < 0) {
            perror("wait");
            exit(1);
        }

        alive--;
    };

    while (next < experiments.size()) {
        while (alive >= MAX_PROCS)
            wait_one();

        pid_t pid = fork();

        if (pid < 0) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            run(experiments[next]);
            exit(0);
        }

        alive++;
        next++;
    }

    while (alive > 0)
        wait_one();

    return 0;
}
