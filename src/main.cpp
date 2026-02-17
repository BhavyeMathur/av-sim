#include "io/RequestsDataframe.h"
#include "io/RidersDataframe.h"

#include "riders/Rider.h"
#include "events/EventBus.h"

#include <iomanip>
#include <thread>


namespace sim {
    thread_local SimulationConfigs configs;

    thread_local mutable_pq<Event> events;
    thread_local std::vector<Request> requests;
    thread_local std::vector<Rider> riders;
}

void create_world(const std::string &config_file) {
    EventBus dispatcher;
    dispatcher.on(EventType::RiderLogin, Rider::on_login);
    dispatcher.on(EventType::RiderLogout, Rider::on_logout);
    dispatcher.on(EventType::RiderWaypoint, Rider::on_waypoint);

    sim::configs = SimulationConfigs("data/sim_configs/" + config_file + ".txt");

    RidersDataFrame riders_df("data/riders/" + sim::configs.get<std::string>("riders") + ".parquet");
    RequestsDataFrame requests_df("data/pings/" + sim::configs.get<std::string>("pings") + ".parquet");

    sim::riders.reserve(sim::riders.size());

    for (const auto &req: requests_df) {
        sim::requests.emplace_back(req);
        sim::events.push({req.created_at, EventType::RequestCreated, OrderCreated{req.id}});
    }

    for (const auto &rider: riders_df) {
        sim::riders.emplace_back(coordinate{static_cast<coordinate_t>(rider.lat),
                                            static_cast<coordinate_t>(rider.lon)});

        sim::events.push({rider.created_at, EventType::RiderLogin, RiderLogin{rider.id}});
        sim::events.push({rider.created_at + rider.lifetime, EventType::RiderLogout, RiderLogout{rider.id}});
    }

    while (!sim::events.empty()) {
        Event event = sim::events.pop();
        dispatcher.dispatch(event);
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
