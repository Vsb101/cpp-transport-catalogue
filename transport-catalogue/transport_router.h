#pragma once

#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <string_view>

#include "graph.h"
#include "router.h"
#include "transport_catalogue.h"

// Настройки маршрута
struct RoutingSettings {
    double bus_wait_time{};     // минуты
    double bus_velocity{};     // км/ч
};

class TransportRouter {
public:
    
    // Описание одного шага в маршруте: например, "ждать 6 мин" или "ехать на автобусе 297, 2 остановки"
    struct RouteData {
        std::string type;           // Тип действия: "Wait" или "Bus"
        std::string stop_name;      // Название остановки, если type == "Wait"
        std::string bus_name;       // Название автобуса, если type == "Bus"
        size_t span_count = 0;      // Количество остановок между началом и концом маршрута (для шага/перегона)
        double time = 0.0;          // Время, которое нужно провести на этом шаге

        // Создаёт "ожидание автобуса" на указанной остановке
        static RouteData Wait(std::string stop_name, double time) {
            return RouteData{
                .type = "Wait",
                .stop_name = std::move(stop_name),
                .bus_name = "",
                .span_count = 0,
                .time = time
            };
        }

        // Создаёт "поездку на автобусе" через указанное число перегонов
        static RouteData Bus(std::string bus_name, size_t span_count, double time) {
            return RouteData{
                .type = "Bus",
                .stop_name = "",
                .bus_name = std::move(bus_name),
                .span_count = span_count,
                .time = time
            };
        }
    };

    using Route = std::vector<RouteData>;
    using RouteInfo = graph::Router<double>::RouteInfo;
    using Graph = graph::DirectedWeightedGraph<double>;

    explicit TransportRouter(double bus_wait_time, double bus_velocity, const TransportCatalogue& catalogue);
 
    std::optional<std::vector<RouteData>> BuildRoute(std::string_view from, std::string_view to) const;

private:
    std::optional<RouteInfo> FindRoute(std::string_view stop_from, std::string_view stop_to) const;
    
    double bus_wait_time_{0.0};
    double bus_velocity_ {0.0};

    // Максимальное число перегонов в маршруте
    // Ограничение на количество поездоек.
    static constexpr size_t MAX_ROUTE_SPANS{90}; 

    Graph graph_;
    std::unique_ptr<graph::Router<double>> router_;

    std::unordered_map<string, graph::VertexId> stop_ids_;
    vector<string> vertex_to_stop_name_;
    Route edge_info_; 
};

class TransportRouterBuilder {
public:
    TransportRouterBuilder() = default;

    // Теперь Build принимает всё: и каталог, и настройки
    TransportRouter Build(const TransportCatalogue& catalogue, const RoutingSettings& settings) const noexcept;

private:
    // Всё задаётся через аргументы Build
};


