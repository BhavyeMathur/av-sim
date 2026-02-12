#pragma once

#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <cassert>
#include <optional>
#include <unordered_map>
#include <unordered_set>


using std::cout, std::endl, std::cerr;

typedef uint32_t order_id_t;
typedef uint32_t rider_id_t;

typedef uint32_t timestamp_t;
typedef uint32_t duration_t;
typedef float distance_t;
typedef float speed_t;

typedef uint16_t cluster_id_t;
typedef uint16_t zone_id_t;
typedef uint16_t customer_id_t;

static constexpr zone_id_t AnyZone = -1;
static constexpr zone_id_t AnyCustomer = -1;

#include "util/coordinate.h"
