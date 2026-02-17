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

void register_default_events() {
    sim::events.on(AllocationEngine::on_request);

    sim::events.on([](const RiderLogin &event) {
        sim::riders[event.rider_id].login();
    });

    sim::events.on([](const RiderLogout &event) {
        sim::riders[event.rider_id].logout();
    });

    sim::events.on([](const RiderWaypoint &event) {
        sim::riders[event.rider_id].complete_waypoint();
    });

    sim::events.on([](const RequestAssigned &event) {
        sim::requests[event.request_id].assign_to(event.rider_id);
    });

    sim::events.on([](const FirstMileStart &event) {
        sim::requests[event.request_id].start_first_mile(event.distance);
    });

    sim::events.on([](const ArrivedAtPickup &event) {
        sim::requests[event.request_id].await_pickup();
    });

    sim::events.on([](const LastMileStart &event) {
        sim::requests[event.request_id].start_last_mile(event.distance);
    });

    sim::events.on([](const ArrivedAtDrop &event) {
        sim::requests[event.request_id].await_drop();
    });

    sim::events.on([](const RequestCompleted &event) {
        sim::requests[event.request_id].mark_completed();
    });
}

void create_requests() {
    RequestsDataFrame requests_df("data/pings/" + sim::configs.get<std::string>("pings") + ".parquet");

    for (const auto &req: requests_df) {
        sim::requests.emplace_back(req);
        sim::events.push({req.created_at, RequestCreated{req.id}});
    }
}

void create_riders() {
    RidersDataFrame riders_df("data/riders/" + sim::configs.get<std::string>("riders") + ".parquet");
    sim::riders.reserve(sim::riders.size());

    for (const auto &r: riders_df) {
        Rider rider(coordinate{static_cast<coordinate_t>(r.lat),
                               static_cast<coordinate_t>(r.lon)});
        sim::riders.emplace_back(rider);

        sim::events.push({r.created_at, RiderLogin{rider.id()}});
        sim::events.push({r.created_at + r.lifetime, RiderLogout{rider.id()}});
    }
}

void create_world(const std::string &config_file) {
    sim::configs = SimulationConfigs("data/sim_configs/" + config_file + ".txt");

    register_default_events();

    create_requests();
    create_riders();

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
