#pragma once

#include "includes.h"

struct Order {
    order_id_t id;
    timestamp_t created_at;

    coordinate pick_coord;
    coordinate drop_coord;

    duration_t pick_time;
    duration_t drop_time;
    duration_t ready_time;
    duration_t sla_time;

    duration_t predicted_ready_time;
    distance_t predicted_lm_dist;

    zone_id_t pick_zone;
    zone_id_t drop_zone;
    customer_id_t customer_id;
};
