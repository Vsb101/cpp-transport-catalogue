#pragma once

#include <string_view>
#include <vector>
#include <deque>
#include <unordered_map>
#include <set>
#include <string>
#include <span>

#include "geo.h"

namespace transport {

using std::string;
using std::string_view;
using std::vector;
using BusList = std::set<std::string_view>; 


struct Stop {
    string name;
    geo::Coordinates coordinates;
};

struct Bus {
    string name;
    std::vector<string_view> stops;
};

struct BusInfo {
    size_t stops_on_route{};
    size_t unique_stops{};
    int route_length{};
    double curvature{};
};

class TransportCatalogue {
public:
    void AddStop(const string& name, geo::Coordinates coordinates);
    void AddBus(const string& name, vector<string_view> stops);
    
    const Stop* FindStop(const string_view stop_name) const;
    const Bus*  FindBus(const string_view bus_name) const;
    
    const BusList& GetBusesForStop(string_view stop_name) const;
    
    const BusInfo GetBusInfo(const string_view bus_name) const;

    void AddDistance(std::string_view from, std::string_view to, int distance);

private:
    struct StopsHasher {
        size_t operator()(const std::pair<const Stop*, const Stop*>& p) const {
            size_t h1 = reinterpret_cast<size_t>(p.first);
            size_t h2 = reinterpret_cast<size_t>(p.second);
            h1 = (h1 >> 16) ^ (h1 << 16);
            h2 = (h2 >> 16) ^ (h2 << 16);
            return h1 ^ (h2 << 1);
        }
    };

    struct StringViewHasher {
        size_t operator()(const string_view& s) const {
            return std::hash<string_view>{}(s);
        }
    };

    std::deque<Stop> stops_;
    std::deque<Bus> buses_;
    std::unordered_map<string_view, const Stop*> stopname_to_stop_;
    std::unordered_map<string_view, const Bus*> busname_to_bus_;
    std::unordered_map<string_view, std::set<string_view>, StringViewHasher> stop_to_buses_;
    std::unordered_map<std::pair<const Stop*, const Stop*>, int, StopsHasher> stops_length_;  
};

} // namespace transport