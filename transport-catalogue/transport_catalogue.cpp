#include "transport_catalogue.h"

#include <stdexcept>
#include <algorithm>

using namespace std;

// Добавляет новую остановку в каталог.
// - Регистрирует остановку в deque stops_.
// - Обновляет map stopname_to_stop_ для быстрого поиска по названию.
// - Инициализирует ассоциацию остановки с маршрутами (stop_to_buses_).
void TransportCatalogue::AddStop(string name, geo::Coordinates position) {
    AddStopImpl({name, position});
}

void TransportCatalogue::AddStopImpl(const Stop& stop) {
    stops_.push_back(stop);
    auto new_stop = &stops_.back();
    stop_to_buses_[new_stop];
    stopname_to_stop_[new_stop->name_] = new_stop;
}

// Добавляет новый маршрут автобуса в каталог.
// - Создает объект Bus с указанным именем.
// - Заполняет маршрут остановками из stopnames.
// - Ассоциирует остановки с маршрутом через AssociateStopWithBus.
// - Обновляет bus_routes_ для быстрого доступа по имени.
void TransportCatalogue::AddRoute(string bus_name, const vector<string_view> &stopnames, bool is_roundtrip) {
    buses_.emplace_back(bus_name);
    Bus *new_bus = bus_routes_[buses_.back().name_] = &buses_.back();
    new_bus->is_roundtrip_ = is_roundtrip;
    new_bus->route_.reserve(stopnames.size());
    for (string_view stopname: stopnames) {
        AssociateStopWithBus(stopname_to_stop_[stopname], new_bus);
        bus_routes_[new_bus->name_]->route_.push_back(stopname_to_stop_[stopname]);
    }
}

// Возвращает маршрут автобуса по его имени.
// - Использует bus_routes_ для поиска.
Bus TransportCatalogue::FindRoute(string_view bus_name) const {
    return *bus_routes_.at(bus_name);
}

// Находит остановку по её названию.
// - Использует stopname_to_stop_ для быстрого поиска.
const Stop& TransportCatalogue::FindStop(string_view stop_name) const {
    return *stopname_to_stop_.at(stop_name);
}

// Считает и возвращает статистику маршрута автобуса.
// - total_stops: общее количество остановок в маршруте.
// - unique_stops: количество уникальных остановок.
// - route_length: реальная длина маршрута (учитывает дороги).
// - curvature: кривизна маршрута (real_length / native_length)
RouteInfo TransportCatalogue::BusRouteInfo(string_view bus_name) const {
    double native_length = CalculateNativeRouteLength(string(bus_name));
    double real_length =   CalculateRealRouteLength(string(bus_name));
    return {
            bus_routes_.at(string(bus_name))->route_.size(),
            CountUniqueRouteStops(string(bus_name)),
            real_length,
            real_length / native_length
    };
}

// Вычисляет реальную длину маршрута (по дорогам).
// - Использует stop_to_near_stop_ для получения расстояний между соседними остановками.
double TransportCatalogue::CalculateRealRouteLength(std::string_view bus_name) const {
    const auto& route = bus_routes_.at(bus_name)->route_;
    if (route.size() < 2) return 0.0;
    double length = 0.0;
    const Stop* from = route[0];
    for (size_t i = 1; i < route.size(); ++i) {
        const Stop* to = route[i];
        auto it = stop_to_near_stop_.find({from, to});
        if (it != stop_to_near_stop_.end()) {
            length += it->second;
        } else {
            length += stop_to_near_stop_.at({to, from});
        }
        from = to;
    }
    return length;
}

// Вычисляет геометрическую длину маршрута (прямая линия между остановками).
// - Использует geo::ComputeDistance для расчёта расстояний.
double TransportCatalogue::CalculateNativeRouteLength(string_view bus_name) const {
    double route_length = 0;
    bool is_first = true;
    geo::Coordinates past_position{};
    for (const auto &stop: bus_routes_.at(bus_name)->route_) {
        if (!is_first) {
            route_length += ComputeDistance(past_position, stop->position_);
        }else{
            is_first = false;
        }
        past_position = stop->position_;
    }
    return route_length;
}

// Считает количество уникальных остановок в маршруте.
// - Использует unordered_set для исключения дубликатов.
size_t TransportCatalogue::CountUniqueRouteStops(string_view bus_name) const {
    size_t unique_stops_count{};
    // Проверяем, есть ли маршрут с указанным именем
    if (bus_routes_.find(bus_name) != bus_routes_.end()) {
        const std::vector<Stop*>&route = bus_routes_.at(bus_name)->route_;
        unique_stops_count = unordered_set<Stop*>(route.begin(), route.end()).size();
    }
    return unique_stops_count;
}

// Ассоциирует остановку с автобусным маршрутом.
// - Добавляет имя маршрута в stop_to_buses_ для указанной остановки.
void TransportCatalogue::AssociateStopWithBus(Stop *stop, const Bus *bus) {
    stop_to_buses_[stop].insert(bus->name_);
}

// Возвращает список маршрутов, проходящих через указанную остановку.
// - Сортирует результат перед возвратом.
TransportCatalogue::SortedBuses TransportCatalogue::StopInfo(std::string_view stop_name) const {
    const auto &buses = stop_to_buses_.at(stopname_to_stop_.at(stop_name));
    return {buses.begin(), buses.end()};
}

// Добавляет расстояние между двумя остановками.
// - Хранит пару остановок и расстояние в stop_to_near_stop_.
void TransportCatalogue::AddDistance(std::string_view stopname_from, std::string_view stopname_to, size_t distance) {
    stop_to_near_stop_[{
        stopname_to_stop_.at(stopname_from),
        stopname_to_stop_.at(stopname_to)
    }] = distance;
}
