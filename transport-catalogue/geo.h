#pragma once

#include <cmath>

namespace geo {

struct Coordinates {
    double lat;
    double lng;
    bool operator==(const Coordinates& other) const {
        return lat == other.lat && lng == other.lng;
    }
    bool operator!=(const Coordinates& other) const {
        return !(*this == other);
    }
};


inline double ComputeDistance(const Coordinates& from, const Coordinates& to) {
    using namespace std;
    if (from == to) { return 0; }

    // Конвертируем градусы в радианы
    constexpr double deg_to_rad = 3.1415926535 / 180.;
    // Радиус Земли в метрах
    constexpr double earth_radius = 6371000.0;

    return acos(sin(from.lat * deg_to_rad) * sin(to.lat * deg_to_rad)
                + cos(from.lat * deg_to_rad) * cos(to.lat * deg_to_rad) * cos(abs(from.lng - to.lng) * deg_to_rad))
        * earth_radius;
}

} // namespace geo