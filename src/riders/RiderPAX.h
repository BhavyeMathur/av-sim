#pragma once

#include "includes.h"

class RiderPAX {
public:
    RiderPAX();

    [[nodiscard]] uint8_t capacity(rider_id_t rider_id) const;

private:
    std::vector<uint8_t> rider_id_to_capacity_;
};
