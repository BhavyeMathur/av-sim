#include "coordinate.h"


coordinate coordinate::lerp(const coordinate &other, float t) const {
    return {lat * (1 - t) + other.lat * t,
            lon * (1 - t) + other.lon * t};
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
