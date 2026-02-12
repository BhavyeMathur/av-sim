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
