#include "Cluster.h"

#include "io/PingDataframe.h"
#include "io/RidersDataframe.h"
#include "io/Statistics.h"
#include "io/SimulationConfigs.h"

#include <tqdm/tqdm.h>


using namespace std;

namespace sim {
    extern thread_local SimulationConfigs configs;
    extern thread_local Statistics stats;
}

Cluster::Cluster()
        : m_riders("data/riders/" + sim::configs.get<std::string>("riders") + ".parquet"),
          m_rider_events(RiderEvents::from_riders(m_riders)),
          m_orders([&] {
              PingDataFrame orders("data/pings/" + sim::configs.get<std::string>("pings") + ".parquet");

              vector<Order> result;
              result.reserve(orders.size());
              for (const auto &order: orders)
                  result.push_back(order);

              return result;
          }())
{

    sim::stats.resize(m_orders.size());
}

void Cluster::simulate() {
    auto until = sim::configs.get<timestamp_t>("length");
    tqdm::tqdm bar(until, 0.05);

    while (sim::clock < until) {
        _update_riders();
        m_alloc_engine.update();

        while (auto order = m_orders.check()) {
            auto allocation = m_alloc_engine.match(*order, m_riders);

            if (allocation.rider_id)
                _on_accept_order(*order, allocation);
            else
                _on_reject_order(*order);
        }

        sim::clock++;
        bar.step();
    }
}

void Cluster::_update_riders() {
    // TODO check if we can assign rider to an unmatched order
    while (auto rider = m_rider_events.logins.check())
        m_riders.spawn(rider->id);

    while (auto rider = m_rider_events.logouts.check())
        m_riders.kill(rider->id);

    m_riders.update();
}

void Cluster::_on_reject_order(const Order &) {

}

void Cluster::_on_accept_order(const Order &order, const AllocationResult &allocation) {
    auto &rider = m_riders[*allocation.rider_id];
    auto result = m_routing_engine.assign_order(allocation, rider, order);
    m_riders.update(rider);

    sim::stats.assign_order(result);
}
