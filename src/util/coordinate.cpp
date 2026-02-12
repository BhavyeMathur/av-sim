#include "coordinate.h"


constexpr distance_t EarthRadius = 6371;

distance_t equirectangular_distance(coordinate_t lat1_rad, coordinate_t lon1_rad,
                                    coordinate_t lat2_rad, coordinate_t lon2_rad) {
    auto dlon = lon2_rad - lon1_rad;
    auto dlat = lat2_rad - lat1_rad;
    auto x = dlon * std::cos((lat1_rad + lat2_rad) / 2);

    return EarthRadius * std::sqrt(x * x + dlat * dlat);
}

coordinate_t coordinate::distance_to(const coordinate &other) const {
    return equirectangular_distance(lat, lon, other.lat, other.lon);
}

coordinate coordinate::lerp(const coordinate &other, float t) const {
    return {lat * (1 - t) + other.lat * t,
            lon * (1 - t) + other.lon * t};
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
