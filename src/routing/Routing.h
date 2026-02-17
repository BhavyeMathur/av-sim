#pragma once

#include "includes.h"

std::pair<distance_t, duration_t> approx_eta(const coordinate &start_pos, const coordinate &dst_pos);

std::pair<distance_t, duration_t> actual_eta(const coordinate &start_pos, const coordinate &dst_pos);
