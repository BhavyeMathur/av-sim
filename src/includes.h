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

#include "util/queues.h"
#include "events/Event.h"

struct Request;

struct Rider;

namespace sim {
    extern thread_local mutable_pq<Event> events;

    extern thread_local std::vector<Request> requests;
    extern thread_local std::vector<Rider> riders;
}
