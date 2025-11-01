#define _USE_MATH_DEFINES
#include "geo.h"

namespace geo {
    
bool Coordinates::operator==(const Coordinates &other) const {
    return lat == other.lat && lng == other.lng;
}

bool Coordinates::operator!=(const Coordinates &other) const {
    return !(*this == other);
}

// Функция возвращает расстояние между двумя географическими координатами
// Используется формула гаверсинусов
// https://habr.com/ru/articles/179157/
double ComputeDistance(const Coordinates& from, const Coordinates& to) {
    if (from == to) { return 0.0; }

    constexpr double EARTH_RADIUS = 6371000.0; // метры
    constexpr double DEGREE_TO_RAD = M_PI / 180.0;

    double dlat = (to.lat - from.lat) * DEGREE_TO_RAD;
    double dlon = (to.lng - from.lng) * DEGREE_TO_RAD;
    double a = pow(sin(dlat / 2), 2) + cos(from.lat * DEGREE_TO_RAD) *
               cos(to.lat * DEGREE_TO_RAD) * pow(sin(dlon / 2), 2);
    double c = 2 * atan2(sqrt(a), sqrt(1 - a));

    return EARTH_RADIUS * c;
}

} // namespace geo