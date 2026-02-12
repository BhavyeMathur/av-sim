#pragma once

#include "includes.h"
#include "config.h"

#include "Order.h"
#include "util/event_vector.h"
#include "riders/RiderEvents.h"
#include "riders/RiderPool.h"
#include "riders/Rider.h"
#include "allocation/AllocationEngine.h"
#include "routing/RoutingEngine.h"
#include "routing/Hotspots.h"


class Cluster {
public:
    Cluster();

    void simulate();

private:
    RiderPool m_riders;
    RiderEvents m_rider_events;
    event_vector<Order, &Order::created_at> m_orders;

    alloc_engine_t m_alloc_engine;
    RoutingEngine m_routing_engine;
//    HotspotEstimator m_hotspots;

    void _update_riders();

    void _on_reject_order(const Order &order);

    void _on_accept_order(const Order &order, const AllocationResult &allocation);
};
