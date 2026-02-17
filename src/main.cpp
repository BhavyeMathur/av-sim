#include "io/RequestsDataframe.h"
#include "io/RidersDataframe.h"

#include "riders/Rider.h"
#include "allocation/AllocationEngine.h"
#include "events/EventBus.h"

#include <iomanip>
#include <thread>


namespace sim {
    thread_local SimulationConfigs configs;
    thread_local EventBus events;

    thread_local timestamp_t clock = 0;

    thread_local std::vector<Request> requests;
    thread_local std::vector<Rider> riders;
}

void create_world(const std::string &config_file) {
    sim::events.on<RiderLogin>(Rider::on_login);
    sim::events.on<RiderLogout>(Rider::on_logout);
    sim::events.on<RiderWaypoint>(Rider::on_waypoint);
    sim::events.on<RequestCreated>(AllocationEngine::on_request);

    sim::configs = SimulationConfigs("data/sim_configs/" + config_file + ".txt");

    RidersDataFrame riders_df("data/riders/" + sim::configs.get<std::string>("riders") + ".parquet");
    RequestsDataFrame requests_df("data/pings/" + sim::configs.get<std::string>("pings") + ".parquet");

    sim::riders.reserve(sim::riders.size());

    for (const auto &req: requests_df) {
        sim::requests.emplace_back(req);
        sim::events.push({req.created_at, RequestCreated{req.id}});
    }

    for (const auto &r: riders_df) {
        Rider rider(coordinate{static_cast<coordinate_t>(r.lat),
                               static_cast<coordinate_t>(r.lon)});
        sim::riders.emplace_back(rider);

        sim::events.push({r.created_at, RiderLogin{rider.id()}});
        sim::events.push({r.created_at + r.lifetime, RiderLogout{rider.id()}});
    }

    while (!sim::events.empty()) {
        Event event = sim::events.pop();
        sim::clock = event.t;
        sim::events.dispatch(event);
    }
}

int main(int argc, char *argv[]) {
    std::cout << std::setprecision(4) << std::fixed;

    std::vector<std::thread> threads;
    for (auto i = 1; i < argc; i++)
        threads.emplace_back([i, &argv]() { create_world(argv[i]); });

    auto s = std::chrono::high_resolution_clock::now();
    for (auto &t: threads)
        t.join();
    auto e = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(e - s);
    std::cout << "Elapsed time: " << duration.count() << " milliseconds\n\n";

    return 0;
}
