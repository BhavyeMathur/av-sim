#pragma once

#include <coordinate.h>

uint64_t latlon_to_h3(coordinate c);

// returns [origin, ring1..., ring2..., ... , ring_max_radius...]
const std::vector<hex_id_t> &hexes_in_increasing_radius(hex_id_t origin, int max_radius);
