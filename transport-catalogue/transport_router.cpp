#include "transport_router.h"

#include <algorithm>

// Для персчета в метры в минуту
constexpr double k_KmhToMpm = 1000.0 / 60.0;  

// Инициализирует граф маршрутов: по 2 вершины на каждую остановку (вход и выход).
// Добавляет рёбра ожидания Wait на всех остановках — фиксированное время.
// Затем добавляет рёбра для всех автобусных маршрутов: от каждой остановки до всех последующих.
// Для каждого ребра вычисляется время в пути на основе расстояния и скорости автобуса.
// Также добавляются обратные рёбра, если маршрут не кольцевой.
// Все рёбра сопровождаются RouteData для последующего восстановления маршрута.
TransportRouter::TransportRouter(double bus_wait_time, double bus_velocity, const TransportCatalogue& catalogue)
    : bus_wait_time_(bus_wait_time), bus_velocity_(bus_velocity) {
    const auto& all_stops = catalogue.GetAllSortedStops();
    const auto& all_buses = catalogue.GetAllSortedBuses();

    // Инициализация графа: 2 вершины на остановку
    graph::DirectedWeightedGraph<double> stops_graph(all_stops.size() * 2);
    vertex_to_stop_name_.resize(all_stops.size() * 2);
    graph::VertexId vertex_id = 0;

    // Этап 1: создание остановок и рёбер ожидания
    for (const auto& [name, stop_ptr] : all_stops) {
        stop_ids_[stop_ptr->name_] = vertex_id;

        // Вход (0) → выход после ожидания
        stops_graph.AddEdge({vertex_id, vertex_id + 1, bus_wait_time_});

        // Wait-ребро
        edge_info_.push_back(RouteData::Wait(stop_ptr->name_, bus_wait_time_));


        vertex_to_stop_name_[vertex_id] = stop_ptr->name_;
        vertex_to_stop_name_[vertex_id + 1] = stop_ptr->name_;

        vertex_id += 2;
    }

    // Этап 2: добавление рёбер маршрутов
    for (const auto& [bus_name, bus_ptr] : all_buses) {
        const auto& stops = bus_ptr->route_;
        if (stops.size() < 2) continue;

        // Добавляем рёбра для поездок от остановки i до j, но не дальше MAX_ROUTE_SPANS перегонов.
        // Это ограничение ускоряет построение графа и работу маршрутизатора.
        // Без него: для автобуса с 1000 остановками будет ~500_000 рёбер (O(n²)).
        // С ним: максимум 20 перегонов вперёд → не более 20 * 1000 = 20_000 рёбер.
        // Пример: если MAX_ROUTE_SPANS = 20, то:
        //   - с остановки 0 можно уехать до 1, 2, ..., 20 (но не до 1000)
        //   - с остановки 50 — до 51, 52, ..., 70
        //   - с остановки 999 — только до 1000 (если есть)
        // Это реалистично: пассажир редко едет более чем на 20 остановок за раз.
        // При этом кратчайший маршрут всё ещё будет найден, так как пассажир может
        // пересесть, если нужно проехать дальше.
        for (size_t i = 0; i < stops.size(); ++i) {
            for (size_t j = i + 1; j < stops.size() && (j - i) <= MAX_ROUTE_SPANS; ++j) {
                const Stop* from = stops[i];
                const Stop* to = stops[j];

                // Расчёт расстояния
                size_t total_distance = 0;
                for (size_t k = i + 1; k <= j; ++k) {
                    total_distance += catalogue.GetDistance(stops[k - 1], stops[k]);
                }

                double travel_time = static_cast<double>(total_distance) / (bus_velocity_ * k_KmhToMpm);

                // Добавляем ребро: выход из "from" → вход в "to"
                stops_graph.AddEdge({
                    stop_ids_.at(from->name_) + 1,
                    stop_ids_.at(to->name_),
                    travel_time
                });

                edge_info_.push_back(RouteData::Bus(std::string(bus_ptr->name_), j - i, travel_time));

                // Обратный путь, если не кольцевой
                if (!bus_ptr->is_roundtrip_) {
                    size_t reverse_distance = 0;
                    for (size_t k = j; k > i; --k) {
                        reverse_distance += catalogue.GetDistance(stops[k], stops[k - 1]);
                    }
                    double reverse_time = static_cast<double>(reverse_distance) / (bus_velocity_ * k_KmhToMpm);

                    stops_graph.AddEdge({
                        stop_ids_.at(to->name_) + 1,
                        stop_ids_.at(from->name_),
                        reverse_time
                    });

                    edge_info_.push_back(RouteData::Bus(
                        std::string(bus_ptr->name_),
                        j - i,
                        reverse_time
                    ));

                }
            }
        }
    }

    // Сохраняем граф
    graph_  = std::move(stops_graph);
    router_ = std::make_unique<graph::Router<double>>(graph_);
}

// Находит кратчайший путь от остановки from до to с помощью через graph::Router.
// Преобразует строковые имена в идентификаторы вершин.
// Начальная вершина — входная (in) остановки from, конечная — входная (in) остановки to.
// Граф строит путь через рёбра ожидания и автобусные перегоны.
// Возвращает nullopt, если одна из остановок не существует.
// Используется для построения RouteData.
std::optional<TransportRouter::RouteInfo> TransportRouter::FindRoute(std::string_view from, std::string_view to) const {
    auto from_it = stop_ids_.find(std::string(from));
    auto to_it   = stop_ids_.find(std::string(to));
    if (from_it == stop_ids_.end() || to_it == stop_ids_.end()) {
        return std::nullopt;
    }

    graph::VertexId from_vertex = from_it->second;  // выходим из ожидания
    graph::VertexId to_vertex   = to_it->second;    // входим на остановку

    return router_->BuildRoute(from_vertex, to_vertex);
}

// Строит человеко-читаемый маршрут: последовательность действий (ожидание, поездка).
// Сначала вызывает FindRoute, чтобы получить путь в терминах рёбер графа.
// Затем по edge_id извлекает RouteData из edge_info_ — например: "ждать 6 мин" или "ехать на 297, 2 остановки".
// Возвращает nullopt, если маршрут не найден.
// Результат используется в JSON-выводе для запроса "Route".
// Это — главный метод, связывающий граф с пользовательским ответом.
std::optional<vector<TransportRouter::RouteData>> TransportRouter::BuildRoute(std::string_view from, std::string_view to) const {
    auto route_info = FindRoute(from, to);
    if (!route_info) { return std::nullopt; }

    Route route;
    route.reserve(route_info->edges.size());

    for (graph::EdgeId edge_id : route_info->edges) {
        route.push_back(edge_info_[edge_id]);
    }

    return route;
}

TransportRouter TransportRouterBuilder::Build(
    const TransportCatalogue& catalogue,
    const RoutingSettings& settings) const noexcept {
    return TransportRouter(settings.bus_wait_time, settings.bus_velocity, catalogue);
}