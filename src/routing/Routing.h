#pragma once

#include "includes.h"

duration_t approx_eta(timestamp_t start_t, const coordinate &start_pos, const coordinate &dst_pos);

duration_t actual_eta(timestamp_t start_t, const coordinate &start_pos, const coordinate &dst_pos);
