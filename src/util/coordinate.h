#pragma once

#include <istream>
#include "includes.h"

typedef float coordinate_t;

struct coordinate {
    coordinate_t lat;
    coordinate_t lon;

    [[nodiscard]] coordinate lerp(const coordinate &other, float t) const;

    [[nodiscard]] coordinate radians() const;

    [[nodiscard]] coordinate degrees() const;

    bool operator==(const coordinate &other) const;

    coordinate operator+(const coordinate &other) const;

    coordinate operator-(const coordinate &other) const;

    coordinate operator+=(const coordinate &other);

    coordinate operator-=(const coordinate &other);

    coordinate operator/(coordinate_t scalar) const;
};

std::istream &operator>>(std::istream &in, coordinate &c);

constexpr double rad2deg(double radians);

constexpr double deg2rad(double degrees);
