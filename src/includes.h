#pragma once

#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <cassert>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <numeric>
#include <array>

#include <coordinate.h>


using std::cout, std::endl, std::cerr;

typedef uint32_t request_id_t;
typedef uint32_t rider_id_t;

typedef uint32_t timestamp_t;
typedef uint32_t duration_t;

typedef float distance_t;
typedef uint64_t hex_id_t;
typedef float speed_t;

static constexpr auto INVALID_HEX_ID = static_cast<hex_id_t >(-1);
static constexpr auto INVALID_RIDER_ID = static_cast<rider_id_t>(-1);

class Request;

enum class _RiderState : uint8_t;

class Rider;

#include "util/queues.h"
#include "events/EventBus.h"
#include "io/SimulationConfigs.h"

namespace sim {
    extern thread_local SimulationConfigs configs;
    extern thread_local EventBus events;

    extern thread_local timestamp_t clock;

    extern thread_local std::vector<Request> requests;
    extern thread_local std::vector<Rider> riders;
}

#if DEBUG
#define debug(...) printf(__VA_ARGS__)
#else
#define debug(...) do {} while (0)
#endif
