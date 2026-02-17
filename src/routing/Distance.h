#pragma once

#include "includes.h"

namespace sim {
    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, coordinate_t lat2_rad, coordinate_t lon2_rad);

    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, coordinate p2);

    distance_t distance(coordinate p1, coordinate_t lat2_rad, coordinate_t lon2_rad);

    distance_t distance(coordinate p1, coordinate p2);
}
