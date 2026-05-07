#include "Routing.h"
#include "Distance.h"

speed_t SPEED_KMPH = 40;
speed_t SPEED_KMPS = SPEED_KMPH / 3600;

float approx_monotonic_in_eta(const coordinate &start_pos, const coordinate &dst_pos) {
    return sim::monotonic_in_distance(start_pos.lat, start_pos.lon, dst_pos.lat, dst_pos.lon);
}

std::pair<distance_t, duration_t> approx_eta(const coordinate &start_pos, const coordinate &dst_pos) {
    if (start_pos == dst_pos)
        return {0, 0};

    auto dist = sim::distance(start_pos, dst_pos);
    return {dist, dist / SPEED_KMPS};
}

std::pair<distance_t, duration_t> actual_eta(const coordinate &start_pos, const coordinate &dst_pos) {
    return approx_eta(start_pos, dst_pos);
}
