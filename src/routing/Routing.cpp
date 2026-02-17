#include "Routing.h"
#include "Distance.h"

speed_t SPEED_KMPH = 40;
speed_t SPEED_KMPS = SPEED_KMPH / 3600;

duration_t approx_eta(timestamp_t start_t, const coordinate &start_pos, const coordinate &dst_pos) {
    return sim::distance(start_pos, dst_pos) / SPEED_KMPS;
}

duration_t actual_eta(timestamp_t start_t, const coordinate &start_pos, const coordinate &dst_pos) {
    return approx_eta(start_t, start_pos, dst_pos);
}
