#pragma once

#include <string_view>
#include <vector>
#include <deque>
#include <unordered_map>
#include <set>
#include <string>

#include "geo.h"

namespace transport {

using BusList = std::vector<std::string_view>;

struct Stop {
    std::string name;
    geo::Coordinates coordinates;
};

struct Bus {
    std::string name;
    std::vector<std::string_view> stops;
};

struct BusInfo {
    size_t stops_on_route{};
    size_t unique_stops{};
    double route_length{};  
};

class TransportCatalogue {
public:
    void AddStop(const std::string& name, geo::Coordinates coordinates);
    void AddBus(const std::string& name, std::vector<std::string_view> stops);
    
    const Stop* FindStop(const std::string_view stop_name) const;
    const Bus*  FindBus(const std::string_view bus_name) const;
    
    // Возвращает список автобусов, проходящих через указанную остановку.
    const BusList GetBusesForStop(std::string_view stop_name) const;
    
    // Возвращает информацию о маршруте автобуса (количество остановок, длина и т.д.).
    const BusInfo GetBusInfo(const std::string_view bus_name) const;

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
        size_t operator()(const std::string_view& s) const {
            return std::hash<std::string_view>{}(s);
        }
    };


    std::deque<Stop> stops_;
    std::deque<Bus> buses_;
    std::unordered_map<std::string_view, const Stop*> stopname_to_stop_;
    std::unordered_map<std::string_view, const Bus*> busname_to_bus_;
    std::unordered_map<std::string_view, std::set<std::string_view>, StringViewHasher> stop_to_buses_;
    std::unordered_map<std::pair<const Stop*, const Stop*>, double, StopsHasher> stops_length_;  
};

} // namespace transport