#pragma once

#include "includes.h"

class ChargingPolicy {
public:
    ChargingPolicy();

    virtual ~ChargingPolicy() = default;

private:
    virtual void on_rider_updated_eta_pos(const RiderUpdatedETAPos &e) = 0;
};

class ChargeInPlace final : public ChargingPolicy {
    void on_rider_updated_eta_pos(const RiderUpdatedETAPos &e) override;
};

class ChargeAtHub final : public ChargingPolicy {
    ChargeAtHub();

    void on_rider_updated_eta_pos(const RiderUpdatedETAPos &e) override;

private:
    std::vector<coordinate> hubs_;
};
