#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <iostream>

#include "transport_catalogue.h"

namespace transport {

// Добавляет новую остановку в каталог.
// - Регистрирует остановку в deque stops_.
// - Обновляет map stopname_to_stop_ для быстрого поиска по названию.
void TransportCatalogue::AddStop(const string& stop_name, geo::Coordinates coordinates) {
    stops_.push_back({stop_name, coordinates});
    stopname_to_stop_[stops_.back().name] = &stops_.back();
}

// Добавляет маршрут автобуса в каталог.
// - Проверяет уникальность имени автобуса.
// - Создаёт объект Bus и добавляет его в buses_.
// - Обновляет индекс stop_to_buses_, связывая остановки с автобусом.
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

// Возвращает указатель на остановку с заданным названием или nullptr, если не найдено.
// Использует map stopname_to_stop_ для быстрого поиска.
const Stop* TransportCatalogue::FindStop(const std::string_view name) const {
    auto it = stopname_to_stop_.find(name);
    return (it != stopname_to_stop_.end()) ? it->second : nullptr;
}

// Возвращает указатель на автобус с заданным названием или nullptr, если не найдено.
// Использует map busname_to_bus_ для быстрого поиска.
const Bus* TransportCatalogue::FindBus(const std::string_view name) const {
    auto it = busname_to_bus_.find(name);
    return (it != busname_to_bus_.end()) ? it->second : nullptr;
}

// Возвращает список всех автобусов, проходящих через указанную остановку.
// - Использует map stop_to_buses_ для получения связанных автобусов.
const BusList& TransportCatalogue::GetBusesForStop(std::string_view stop_name) const {
    static const BusList empty_set;
    auto it = stop_to_buses_.find(stop_name);
    return (it != stop_to_buses_.end()) ? it->second : empty_set;
}

// Вычисляет и возвращает статистику маршрута автобуса.
// - Включает общее количество остановок, уникальных остановок, длину маршрута и кривизну.
// - Рассчитывает как геометрическое расстояние (прямая линия), так и фактическую длину маршрута.
// - Учитывает отсутствующие расстояния, проверяя оба направления в stops_length_.
const BusInfo TransportCatalogue::GetBusInfo(const std::string_view bus_name) const {
    BusInfo result = {};
    const Bus* bus = FindBus(bus_name);
    if (!bus) { return {}; }

    double total_geo_distance{};
    int total_distance{};

    for (size_t i = 1; i < bus->stops.size(); ++i) {
        const Stop* from = FindStop(bus->stops[i - 1]);
        const Stop* to = FindStop(bus->stops[i]);

        // Рассчитывает прямое (геометрическое) расстояние
        total_geo_distance += geo::ComputeDistance(from->coordinates, to->coordinates);

        // Ищем фактическое расстояние в любом направлении
        if (auto it = stops_length_.find({from, to}); it != stops_length_.end()) {
            total_distance += it->second;
        } else if (auto it = stops_length_.find({to, from}); it != stops_length_.end()) {
            total_distance += it->second;
        }
    }
    
    result.stops_on_route = bus->stops.size();
    std::unordered_set<std::string_view> unique_stops(bus->stops.begin(), bus->stops.end());
    result.unique_stops = unique_stops.size();
    result.route_length = total_distance;
    result.curvature = total_distance/total_geo_distance;

    return result;
}

// Добавляет информацию о расстоянии между двумя остановками в stops_length_.
void TransportCatalogue::AddDistance(std::string_view from, std::string_view to, int distance) {
    const Stop* from_stop = stopname_to_stop_.at(from);
    const Stop* to_stop = stopname_to_stop_.at(to);
    stops_length_.insert({{from_stop, to_stop}, distance});
}

} // namespace transport