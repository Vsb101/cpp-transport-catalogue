#include "domain.h"
#include <algorithm>
#include <utility>
#include <functional>

// Конструкторы Stop
Stop::Stop(std::string_view name) : name_(std::string(name)) {}
Stop::Stop(std::string_view name, geo::Coordinates position)
    : name_(std::string(name)), position_(position) {}

// Оператор сравнения Stop
bool Stop::operator==(const Stop& stop) const {
    return name_ == stop.name_ && ApproximatelyEquals(position_, stop.position_);
}

// Вспомогательная функция для приблизительного сравнения координат
inline bool ApproximatelyEquals(const geo::Coordinates& lhs, const geo::Coordinates& rhs) {
    constexpr double epsilon = 1e-9;
    return std::abs(lhs.lat - rhs.lat) <= epsilon &&
           std::abs(lhs.lng - rhs.lng) <= epsilon;
}

// Конструкторы Bus
Bus::Bus(std::string_view name) : name_(std::string(name)) {}
Bus::Bus(std::string_view name, const std::vector<Stop*>& route)
    : name_(std::string(name)), route_(route) {}

// Оператор сравнения Bus
bool Bus::operator==(const Bus& bus) const {
    return name_ == bus.name_ &&
           std::equal(route_.begin(), route_.end(),
                      bus.route_.begin(), bus.route_.end());
}

// Хэш-функция для пары указателей
size_t PairStopHasher::operator()(const std::pair<const Stop*, const Stop*>&p) const{
    using std::hash;
    return hash<size_t>()((size_t)p.first) ^ (hash<size_t>()((size_t)p.second) << 1);
}

// Операторами равенства пар указателей
bool PairStopEqual::operator()(const std::pair<const Stop*, const Stop*>& a, const std::pair<const Stop*, const Stop*>& b) const {
    return a.first == b.first && a.second == b.second;
}

bool BusnameComparator::operator()(const Bus &lhs, const Bus &rhs) const{
    return lhs.name_ < rhs.name_;
}

bool StopnameComparator::operator()(const Stop *lhs, const Stop *rhs) const {
    return lhs->name_ < rhs->name_;
}
