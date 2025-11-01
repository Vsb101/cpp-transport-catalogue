#pragma once

#include "geo.h"

#include <string>
#include <vector>
#include <string_view>

struct Stop {
    std::string name_;
    geo::Coordinates position_;

    Stop() = default;
    explicit Stop(std::string_view name);
    Stop(std::string_view name, geo::Coordinates position);

    bool operator==(const Stop& stop) const;
};

struct Bus {
    std::string name_;
    std::vector<Stop*> route_;
    bool is_roundtrip_{};

    Bus() = default;
    explicit Bus(std::string_view name);
    Bus(std::string_view name, const std::vector<Stop*>& route);

    bool operator==(const Bus& bus) const;
};

struct RouteInfo {
    size_t total_stops;
    size_t unique_stops;
    double length;
    double curvature;
};

struct PairStopHasher{
    size_t operator()(const std::pair<const Stop*, const Stop*>& p) const;
};

struct PairStopEqual {
    bool operator()(const std::pair<const Stop*, const Stop*>& a,
                    const std::pair<const Stop*, const Stop*>& b) const;
};

struct BusnameComparator {
    bool operator()(const Bus& lhs, const Bus& rhs) const;
};

struct StopnameComparator {
    bool operator()(const Stop *lhs, const Stop *rhs) const;
};

inline bool ApproximatelyEquals(const geo::Coordinates& lhs, const geo::Coordinates& rhs);