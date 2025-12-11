#include "transport_catalogue.h"
#include <algorithm>
#include <unordered_set>

using namespace std;

void TransportCatalogue::AddStop(string name, geo::Coordinates position) {
    AddStopImpl({move(name), position});
}

void TransportCatalogue::AddStopImpl(const Stop& stop) {
    stops_.push_back(stop);
    auto new_stop = &stops_.back();
    stop_to_buses_[new_stop];
    stopname_to_stop_[new_stop->name_] = new_stop;
}

void TransportCatalogue::AddRoute(string bus_name, const vector<string_view> &stopnames, bool is_roundtrip) {
    buses_.emplace_back(move(bus_name));
    Bus *new_bus = &buses_.back();
    new_bus->is_roundtrip_ = is_roundtrip;
    new_bus->route_.reserve(stopnames.size());
    
    for (string_view stopname : stopnames) {
        auto stop_it = stopname_to_stop_.find(stopname);
        if (stop_it == stopname_to_stop_.end()) {
            // Игнорируем неизвестные остановки
            continue;
        }
        Stop* stop = stop_it->second;
        AssociateStopWithBus(stop, new_bus);
        new_bus->route_.push_back(stop);
    }

    bus_routes_[new_bus->name_] = new_bus;
}

Stop* TransportCatalogue::FindStop(string_view stop_name) const {
    auto it = stopname_to_stop_.find(stop_name);
    return (it != stopname_to_stop_.end()) ? it->second : nullptr;
}

const Bus* TransportCatalogue::FindRoute(string_view bus_name) const {
    auto it = bus_routes_.find(bus_name);
    return (it != bus_routes_.end()) ? it->second : nullptr;
}

std::optional<RouteInfo> TransportCatalogue::BusRouteInfo(string_view bus_name) const {
    const Bus* bus = FindRoute(bus_name);
    if (!bus) {
        return std::nullopt;
    }
    double native_length = CalculateNativeRouteLength(bus);
    double real_length = CalculateRealRouteLength(bus);
    return RouteInfo{
        bus->route_.size(),
        CountUniqueRouteStops(bus),
        real_length,
        real_length / native_length
    };
}

double TransportCatalogue::CalculateRealRouteLength(const Bus* bus) const {
    if (!bus || bus->route_.size() < 2) {
        return 0.0;
    }
    double length = 0.0;
    const Stop* from = bus->route_[0];
    for (size_t i = 1; i < bus->route_.size(); ++i) {
        const Stop* to = bus->route_[i];
        auto it = stop_to_near_stop_.find({from, to});
        if (it != stop_to_near_stop_.end()) {
            length += it->second;
        } else {
            auto rev_it = stop_to_near_stop_.find({to, from});
            if (rev_it != stop_to_near_stop_.end()) {
                length += rev_it->second;
            } else {
                // Если расстояние не задано, используем евклидово
                length += ComputeDistance(from->position_, to->position_);
            }
        }
        from = to;
    }
    return length;
}

double TransportCatalogue::CalculateNativeRouteLength(const Bus* bus) const {
    if (!bus || bus->route_.size() < 2) {
        return 0.0;
    }
    double route_length = 0.0;
    for (size_t i = 1; i < bus->route_.size(); ++i) {
        route_length += ComputeDistance(
            bus->route_[i-1]->position_,
            bus->route_[i]->position_
        );
    }
    return route_length;
}

size_t TransportCatalogue::CountUniqueRouteStops(const Bus* bus) const {
    if (!bus) { return 0; }
    unordered_set<const Stop*> unique_stops;
    for (const auto* stop : bus->route_) {
        unique_stops.insert(stop);
    }
    return unique_stops.size();
}

size_t TransportCatalogue::GetDistance(const Stop *from, const Stop *to) const {
    if (stop_to_near_stop_.count({from, to})) {
        return stop_to_near_stop_.at({from, to});
    }
    if (stop_to_near_stop_.count({to, from})) {
        return stop_to_near_stop_.at({to, from});
    }
    return 0;
}

void TransportCatalogue::AssociateStopWithBus(Stop *stop, const Bus *bus) {
    stop_to_buses_[stop].insert(bus->name_);
}

TransportCatalogue::SortedBuses TransportCatalogue::StopInfo(string_view stop_name) const {
    Stop* stop = FindStop(stop_name);
    if (!stop) {
        return {};
    }
    auto it = stop_to_buses_.find(stop);
    if (it == stop_to_buses_.end()) {
        return {};
    }
    return {it->second.begin(), it->second.end()};
}

void TransportCatalogue::AddDistance(string_view stopname_from, string_view stopname_to, size_t distance) {
    const Stop* from = FindStop(stopname_from);
    const Stop* to   = FindStop(stopname_to);
    if (!from || !to) { return; }
    stop_to_near_stop_[{from, to}] = distance;
}

map<string_view, const Bus *> TransportCatalogue::GetAllSortedBuses() const noexcept {
    map<string_view, const Bus *> result;
    for (const auto &bus: bus_routes_) {
        result.emplace(bus);
    }
    return result;
}

map<string_view, const Stop *> TransportCatalogue::GetAllSortedStops() const noexcept {
    map<string_view, const Stop *> result;
    for (const auto &stop: stopname_to_stop_) {
        result.emplace(stop);
    }
    return result;
}

