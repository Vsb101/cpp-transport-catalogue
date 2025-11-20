#pragma once

#include <string>
#include <cstdint>
#include <optional>

#include <deque>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

#include "domain.h"
#include "geo.h"

using std::string_view;
using std::unordered_set;
using std::set;
using std::unordered_map;
using std::pair;
using std::vector;
using std::deque;
using std::string;

class TransportCatalogue {
    
    using Buses = unordered_set<string_view>;
    using SortedBuses = set<string_view>;
    using DistanceMap = unordered_map<pair<const Stop*, const Stop*>, size_t, PairStopHasher, PairStopEqual>;

public:
    void AddStop(string name, geo::Coordinates position);
    void AddDistance(string_view stopname_from, string_view stopname_to, size_t distance);
    void AddRoute(string bus_name, const vector<string_view>& stopnames, bool is_roundtrip);
    Stop* FindStop(string_view stop_name) const;
    const Bus* FindRoute(string_view bus_name) const;
    std::optional<RouteInfo> BusRouteInfo(string_view bus_name) const;
    SortedBuses StopInfo(string_view stop_name) const;

private:
    void AssociateStopWithBus(Stop *stop, const Bus *bus);
    void AddStopImpl(const Stop &stop);
    double CalculateRealRouteLength(const Bus* bus) const;
    double CalculateNativeRouteLength(const Bus* bus) const;
    size_t CountUniqueRouteStops(const Bus* bus) const;
    
    deque<Stop> stops_;
    deque<Bus>  buses_;
    unordered_map<string_view, Stop*> stopname_to_stop_;
    unordered_map<Stop*, Buses>       stop_to_buses_;
    unordered_map<string_view, Bus*>  bus_routes_;
    DistanceMap stop_to_near_stop_;
};
