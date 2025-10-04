#include <algorithm>

#include "transport_catalogue.h"

namespace transport {

using std::string;
using std::string_view;
using std::move;
using std::vector;

void TransportCatalogue::AddStop(const string& stop_name, geo::Coordinates coordinates) {
    stops_.push_back({stop_name, coordinates});
    stopname_to_stop_[stops_.back().name] = &stops_.back();
}

void TransportCatalogue::AddBus(const string& bus_name, const vector<string_view> stops) {
    // Конвертируем остановки в string_view
    std::vector<string_view> stops_to_string;
    for (const auto& stop : stops) {
        auto it = stopname_to_stop_.find(stop);
        if (it != stopname_to_stop_.end()) {
            stops_to_string.push_back(it->second->name);
        }
    }

    // Проверяем, существует ли автобус с таким именем
    if (busname_to_bus_.find(bus_name) != busname_to_bus_.end()) {
        return;
    }

    // Добавляем автобус в список
    buses_.push_back({bus_name, std::move(stops_to_string)});
    busname_to_bus_[buses_.back().name] = &buses_.back();

    // Обновляем индекс: добавляем автобус к каждой остановке его маршрута
    for (const auto& stop : stops) {
        stop_to_buses_[stop].insert(std::string_view(bus_name));
    }
}

const Stop* TransportCatalogue::FindStop(const std::string_view name) const {
    auto it = stopname_to_stop_.find(name);
    return (it != stopname_to_stop_.end()) ? it->second : nullptr;
}

const Bus* TransportCatalogue::FindBus(const std::string_view name) const {
    auto it = busname_to_bus_.find(name);
    return (it != busname_to_bus_.end()) ? it->second : nullptr;
}


const vector<std::string_view> TransportCatalogue::GetBusesForStop(std::string_view stop_name) const {
    auto it = stop_to_buses_.find(stop_name);
    if (it == stop_to_buses_.end()) {
        return {};
    }

    std::vector<std::string_view> result(it->second.begin(), it->second.end());
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