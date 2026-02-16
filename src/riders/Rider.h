#pragma once

#include "includes.h"
#include <coordinate.h>


struct Rider {
    rider_id_t id;

    coordinate pos;
    coordinate eta_pos;
};
