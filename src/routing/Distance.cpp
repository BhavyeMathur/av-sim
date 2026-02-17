#include "Distance.h"

constexpr distance_t EarthRadius = 6371;

distance_t equirectangular_distance(coordinate_t lat1_rad, coordinate_t lon1_rad,
                                    coordinate_t lat2_rad, coordinate_t lon2_rad) {
    auto dlon = lon2_rad - lon1_rad;
    auto dlat = lat2_rad - lat1_rad;
    auto x = dlon * std::cos((lat1_rad + lat2_rad) / 2);

    return EarthRadius * std::sqrt(x * x + dlat * dlat);
}

namespace sim {
    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, coordinate_t lat2_rad, coordinate_t lon2_rad) {
        return static_cast<distance_t>(0.572)
               + static_cast<distance_t>(1.273) * equirectangular_distance(lat1_rad, lon1_rad, lat2_rad, lon2_rad);
    }

    distance_t distance(coordinate_t lat1_rad, coordinate_t lon1_rad, coordinate p2) {
        return distance(lat1_rad, lon1_rad, p2.lat, p2.lon);
    }

    distance_t distance(coordinate p1, coordinate_t lat2_rad, coordinate_t lon2_rad) {
        return distance(p1.lat, p1.lon, lat2_rad, lon2_rad);;
    }

    distance_t distance(coordinate p1, coordinate p2) {
        return distance(p1.lat, p1.lon, p2.lat, p2.lon);
    }
}
