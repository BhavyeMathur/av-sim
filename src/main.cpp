#include "Cluster.h"
#include "io/Statistics.h"
#include "io/SimulationConfigs.h"
#include "routing/EVCharging.h"

#include <iomanip>
#include <thread>


namespace sim {
    thread_local timestamp_t clock = 0;

    thread_local SimulationConfigs configs;
    thread_local Statistics stats;
    thread_local ZoneClusters zones(false);

    #if SIM_FEATURE_EV_CHARGING
    thread_local EVCharging ev_charging(false);
    #endif
}

int main(int argc, char *argv[]) {
    std::cout << std::setprecision(4) << std::fixed;

    auto n_clusters = argc - 1;

    std::vector<std::thread> threads;
    threads.reserve(n_clusters);

    std::vector<Cluster> clusters;
    clusters.reserve(n_clusters);

    for (auto i = 0; i < n_clusters; i++)
        threads.emplace_back([i, &argv]() {
            sim::configs = SimulationConfigs("data_sim/sim_configs/" + std::string(argv[i + 1]) + ".txt");
            sim::zones = ZoneClusters();

            #if SIM_FEATURE_EV_CHARGING
            sim::ev_charging = EVCharging();
            #endif

            Cluster c;
            c.simulate();
            sim::stats.save("output/" + sim::configs.get<std::string>("output") + ".parquet");
        });

    auto s = std::chrono::high_resolution_clock::now();
    for (auto &t: threads)
        t.join();
    auto e = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(e - s);
    std::cout << "Elapsed time: " << duration.count() << " milliseconds\n\n";

    return 0;
}
