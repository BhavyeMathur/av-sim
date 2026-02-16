#pragma once

#include "includes.h"
#include <coordinate.h>


struct Request {
    order_id_t id;
    timestamp_t created_at;

    coordinate pick_coord;
    coordinate drop_coord;
    distance_t predicted_lm_dist;
};
