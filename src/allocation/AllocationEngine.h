#pragma once

#include "includes.h"
#include "AllocationEngineConfig.h"

struct Order;

class RiderPool;

struct AllocationResult {
    std::optional<rider_id_t> rider_id{std::nullopt};
};

class AllocationEngine {
public:
    virtual AllocationResult match(const Order &order, const RiderPool &riders) = 0;

    virtual void update() {
    };
};

class SimpleJIT final : public AllocationEngine {
public:
    SimpleJIT();

    AllocationResult match(const Order &order, const RiderPool &riders) override;

private:
    float fm_cutoff_km;
    float speed_kmps = 40.0 / 3600;
};

class ClusterJIT final : public AllocationEngine {
public:
    explicit ClusterJIT();

    AllocationResult match(const Order &order, const RiderPool &riders) override;

    void update() override;

private:
    AllocationEngineConfig<FirstMileConfig> fm_config;
    AllocationEngineConfig<LastMileConfig> lm_config;
    AllocationEngineConfig<SLAConfig> sla_config;
    AllocationEngineConfig<RPHConfig> rph_config;
};
