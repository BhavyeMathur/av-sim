#pragma once

#include <istream>
#include "includes.h"

typedef float coordinate_t;

distance_t equirectangular_distance(coordinate_t lat1_rad, coordinate_t lon1_rad,
                                    coordinate_t lat2_rad, coordinate_t lon2_rad);

struct coordinate {
    coordinate_t lat;
    coordinate_t lon;

    [[nodiscard]] distance_t distance_to(const coordinate &other) const;

    [[nodiscard]] coordinate lerp(const coordinate &other, float t) const;

    coordinate operator+(const coordinate &other) const;

    coordinate operator-(const coordinate &other) const;

    coordinate operator+=(const coordinate &other);

    coordinate operator-=(const coordinate &other);

    coordinate operator/(coordinate_t scalar) const;
};

std::istream &operator>>(std::istream &in, coordinate &c);
