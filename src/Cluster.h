#pragma once

#include "includes.h"

#include "Order.h"
#include "util/event_vector.h"
#include "riders/RiderEvents.h"
#include "riders/RiderPool.h"
#include "riders/Rider.h"
#include "allocation/AllocationEngine.h"
#include "routing/RoutingEngine.h"


class Cluster {
public:
    Cluster();

    void simulate();

private:
    RiderPool m_riders;
    RiderEvents m_rider_events;
    event_vector<Order, &Order::created_at> m_orders;

    AllocationEngine m_alloc_engine;
    RoutingEngine m_routing_engine;

    void _update_riders();

    void _on_reject_order(const Order &order);

    void _on_accept_order(const Order &order, const AllocationResult &allocation);
};
