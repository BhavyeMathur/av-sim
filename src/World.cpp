#include "World.h"

#include "extern.h"

#include "io/RequestsDataframe.h"
#include "io/RidersDataframe.h"

#include "riders/FleetStats.h"
#include "riders/RiderStats.h"

#include "policy/BestPickupStrategy.h"
#include "policy/Charging.h"

#include <pandas.h>
#include <tqdm.h>


namespace sim {
    thread_local SimulationConfigs configs;
    thread_local EventBus events;

    thread_local timestamp_t clock = 0;

    thread_local std::vector<Request> requests;
    thread_local std::vector<Rider> riders;

    thread_local RiderBattery rider_battery;
    thread_local RiderPAX rider_pax;
}

void register_default_events() {
    sim::events.on([](const RiderLogin &event) {
        sim::riders[event.rider_id].login();
    });

    sim::events.on([](const RiderLogout &event) {
        sim::riders[event.rider_id].logout();
    });

    sim::events.on([](const RiderWaypoint &event) {
        debug("RiderWaypoint(rider_id=%i)\n", event.rider_id);
        sim::riders[event.rider_id].complete_waypoint();
    });

    sim::events.on([](const RequestAssigned &event) {
        sim::riders[event.rider_id].assign_request();
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
    RequestsDataFrame requests_df(sim::configs.sim.requests_file);

    for (const auto &req: requests_df) {
        if (req.created_at >= sim::configs.sim.length_s)
            break;

        sim::requests.emplace_back(req);
        sim::events.push({req.created_at, RequestCreated{req.id}});
    }
}

void create_riders() {
    RidersDataFrame riders_df(sim::configs.sim.riders_file);
    sim::riders.reserve(sim::riders.size());

    for (const auto &r: riders_df) {
        Rider rider(coordinate{static_cast<coordinate_t>(r.lat),
                               static_cast<coordinate_t>(r.lon)});
        sim::riders.emplace_back(rider);

        sim::events.push({r.created_at, RiderLogin{rider.id()}});

        // TODO remove if logoffs needed
        // sim::events.push({r.created_at + r.lifetime, RiderLogout{rider.id()}});
    }
}

std::unique_ptr<Strategy> get_allocation_engine() {
    const auto &strategy = sim::configs.policy.matching;

    if (strategy == "global")
        return std::make_unique<GlobalBestPickupStrategy>();
    else if (strategy == "greedy-h3")
        return std::make_unique<GreedyH3BestPickupStrategy>();
    else if (strategy == "bounded-h3")
        return std::make_unique<BoundedH3BestPickupStrategy>();
    else if (strategy == "ringed-h3")
        return std::make_unique<RingedH3BestPickupStrategy>();

    throw std::invalid_argument("unknown strategy");
}

std::unique_ptr<ChargingPolicy> get_charging_policy() {
    const auto &strategy = sim::configs.policy.charging;

    if (strategy == "in-place")
        return std::make_unique<ChargeInPlace>();
    else if (strategy == "closest-hub")
        return std::make_unique<ChargeAtHub>();
    else if (strategy == "disable")
        return nullptr;

    throw std::invalid_argument("unknown charging policy");
}

void save() {
    auto n = sim::requests.size();
    printf("...saving statistics (count=%zu)\n", n);

    std::vector<bool> accepted;
    std::vector<rider_id_t> assigned_to;
    std::vector<distance_t> fm_dist;
    std::vector<distance_t> lm_dist;

    std::vector<timestamp_t> assigned_at;
    std::vector<timestamp_t> start_at;
    std::vector<timestamp_t> arrived_pickup_at;
    std::vector<timestamp_t> pickedup_at;
    std::vector<timestamp_t> arrived_drop_at;
    std::vector<timestamp_t> completed_at;

    accepted.resize(n);
    assigned_to.resize(n);
    fm_dist.resize(n);
    lm_dist.resize(n);

    assigned_at.resize(n);
    start_at.resize(n);
    arrived_pickup_at.resize(n);
    pickedup_at.resize(n);
    arrived_drop_at.resize(n);
    completed_at.resize(n);

    for (size_t i = 0; i < n; i++) {
        auto &req = sim::requests[i];

        accepted[i] = req.state() != Request::State::Unassigned;
        if (!accepted[i])
            continue;

        assigned_to[i] = req.assigned_rider();
        fm_dist[i] = req.assigned_fm_dist();
        lm_dist[i] = req.assigned_lm_dist();

        assigned_at[i] = req.assigned_at();
        start_at[i] = req.started_at();
        arrived_pickup_at[i] = req.arrived_pickup_at();
        pickedup_at[i] = req.pickedup_at();
        arrived_drop_at[i] = req.arrived_drop_at();
        completed_at[i] = req.completed_at();
    }

    auto table = pd::make_table(pd::col("completed", accepted),
                                pd::col("fm_dist", fm_dist),
                                pd::col("lm_dist", lm_dist),
                                pd::col("assigned_at", assigned_at),
                                pd::col("start_at", start_at),
                                pd::col("arrived_pickup_at", arrived_pickup_at),
                                pd::col("pickedup_at", pickedup_at),
                                pd::col("arrived_drop_at", arrived_drop_at),
                                pd::col("completed_at", completed_at),
                                pd::col("assigned_rider", assigned_to));

    auto filepath = sim::configs.sim.output + "requests.parquet";
    pd::write_table_to_parquet(table, filepath);
}

void create_world(const std::string &config_file) {
    sim::configs = load_config(config_file);

    register_default_events();

    create_requests();
    create_riders();

    auto alloc_engine = get_allocation_engine();
    auto charging_policy = get_charging_policy();

    FleetStats();
    RiderStats();

    sim::rider_battery.init();
    sim::rider_pax.init();

    // ------------------

    sim::events.trigger(SimStart{});

    auto max_size = sim::events.size();
    unsigned int i = 0;
    tqdm::tqdm bar(100);

    while (!sim::events.empty()) {
        if (i == 0) {
            max_size = std::max(max_size, sim::events.size());
            bar.update(100 - (100.0f * sim::events.size()) / max_size);

            i = 1000;
        }
        i--;

        Event event = sim::events.pop();
        sim::clock = event.t;
        sim::events.dispatch(event);
    }
    bar.complete();

    printf("simulation complete\n");
    sim::events.trigger(SimComplete{});
    save();
}
