#include <algorithm>

#include "transport_catalogue.h"

namespace transport {

using std::string;
using std::string_view;
using std::move;
using std::vector;

void TransportCatalogue::AddStop(const string& stop_name, geo::Coordinates coordinates) {
    stops_.push_back({move(stop_name), move(coordinates)});
    stopname_to_stop_[stops_.back().name] = &stops_.back();
}

void TransportCatalogue::AddBus(const string& bus_name, vector<string_view> stops) {
    std::vector<string_view> stops_to_string;
    for (const string_view stop : stops) {
        stops_to_string.push_back(stopname_to_stop_.at(stop)->name);
    }
    buses_.push_back({move(bus_name), move(stops_to_string)});
    busname_to_bus_[buses_.back().name] = &buses_.back();
}

const Stop* TransportCatalogue::FindStop(const std::string_view& name) const {
    auto it = stopname_to_stop_.find(name);
    return (it != stopname_to_stop_.end()) ? it->second : nullptr;
}

const Bus* TransportCatalogue::FindBus(const std::string_view& name) const {
    auto it = busname_to_bus_.find(name);
    return (it != busname_to_bus_.end()) ? it->second : nullptr;
}

const vector<std::string_view> TransportCatalogue::GetBusesForStop (std::string_view stop_name) const {
    const Stop* stop  = FindStop(stop_name);
    if (!stop) {
        return {};
    }
    std::vector<std::string_view> result;
    for (const auto& bus : buses_) {
        for (const auto& bus_stop : bus.stops) {
            if (bus_stop == stop->name) {
                result.push_back(bus.name);
                break;
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

const BusInfo TransportCatalogue::GetBusInfo(const std::string_view bus_name) const {
    BusInfo result = {};
    const Bus* bus = FindBus(bus_name);
    if (!bus) {
        return {};
    }

    result.stops_on_route = bus->stops.size();

    std::unordered_set<std::string_view> unique_stops(bus->stops.begin(), bus->stops.end());
    result.unique_stops = unique_stops.size();

    if (result.stops_on_route > 1) {
        double total_distance = 0.0;
        for (size_t i = 1; i < bus->stops.size(); ++i) {
            std::string_view from_stop = bus->stops[i - 1];
            std::string_view to_stop = bus->stops[i];

            const Stop* from = FindStop(from_stop);
            const Stop* to = FindStop(to_stop);

            if (from && to) {
                auto it = stops_length_.find({from, to});
                if (it != stops_length_.end()) {
                    total_distance += it->second;
                } else {
                    // Если расстояние не найдено, вычислим его
                    total_distance += geo::ComputeDistance(from->coordinates, to->coordinates);
                }
            }
        }
        result.route_length = total_distance;
    }

    return result;
}

} // namespace transport