#pragma once

#include <cstdint>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <cassert>
#include <optional>
#include <numeric>
#include <array>

#include <coordinate.h>


using std::cout, std::endl, std::cerr;

typedef uint32_t request_id_t;
typedef uint32_t rider_id_t;

typedef uint32_t timestamp_t;
typedef uint32_t duration_t;

typedef float distance_t;
typedef uint64_t cell_id_t;
typedef float speed_t;

static constexpr auto INVALID_CELL_ID = static_cast<cell_id_t >(-1);
static constexpr auto INVALID_REQ_ID = static_cast<request_id_t>(-1);
static constexpr auto INVALID_RIDER_ID = static_cast<rider_id_t>(-1);

enum class _RiderState : uint8_t;

class Rider;

class Request;

#include "events/Event.h"
#include "io/SimulationConfigs.h"
#include "util/lock.h"

namespace sim {
    extern SimulationConfigs configs;

    extern thread_local timestamp_t clock;

    extern std::vector<Rider> riders;
    extern std::vector<spinlock> rider_mutexes;
    extern size_t n_riders;
}

#if DEBUG
#define debug(...) \
    do { \
        printf("[thread %zu] ", std::hash<std::thread::id>{}(std::this_thread::get_id())); \
        printf(__VA_ARGS__); \
        fflush(stdout); \
    } while (false)
#else
#define debug(...) do {} while (0)
#endif

#if defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#else
#define FORCE_INLINE inline
#endif
