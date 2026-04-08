#include "coordinate.h"
#include <numbers>


coordinate coordinate::lerp(const coordinate &other, float t) const {
    return {lat * (1 - t) + other.lat * t,
            lon * (1 - t) + other.lon * t};
}

coordinate coordinate::radians() const {
    return {static_cast<coordinate_t>(deg2rad(lat)),
            static_cast<coordinate_t>(deg2rad(lon))};
}

coordinate coordinate::degrees() const {
    return {static_cast<coordinate_t>(rad2deg(lat)),
            static_cast<coordinate_t>(rad2deg(lon))};
}

bool coordinate::operator==(const coordinate &other) const {
    return lat == other.lat and lon == other.lon;
}

coordinate coordinate::operator+(const coordinate &other) const {
    return {lat + other.lat, lon + other.lon};
}

coordinate coordinate::operator-(const coordinate &other) const {
    return {lat - other.lat, lon - other.lon};
}

coordinate coordinate::operator+=(const coordinate &other) {
    lat += other.lat;
    lon += other.lon;
    return *this;
}

coordinate coordinate::operator-=(const coordinate &other) {
    lat -= other.lat;
    lon -= other.lon;
    return *this;
}

coordinate coordinate::operator/(coordinate_t scalar) const {
    return {lat / scalar, lon / scalar};
}

std::istream &operator>>(std::istream &in, coordinate &c) {
    return in >> c.lat >> c.lon;
}

constexpr double rad2deg(double radians) {
    return radians * (180.0 / std::numbers::pi);
}

constexpr double deg2rad(double degrees) {
    return degrees * (std::numbers::pi / 180.0);
}
