#include "H3.h"
#include "h3api.h"


static constexpr int H3_RESOLUTION = 7;

uint64_t latlon_to_h3(coordinate c) {
    LatLng g{c.lat, c.lon};

    H3Index res;
    latLngToCell(&g, H3_RESOLUTION, &res);
    return res;
}
