#pragma once

#include <memory>
#include <map>

#include "graph.h"
#include "router.h"
#include "transport_catalogue.h"

using std::string_view;
using std::optional;
using std::string;
using std::unique_ptr;
using std::map;

struct RoutingSettings {
    double bus_wait_time{};     // минуты
    double bus_velocity{};     // км/ч
};

class TransportRouter {
public:
    struct RouteData {
        string type;
        string stop_name;
        string bus_name;
        size_t span_count = 0;
        double time = 0.0;

        static RouteData Wait(std::string stop_name, double time) {
            return RouteData{
                .type = "Wait",
                .stop_name = std::move(stop_name),
                .bus_name = "",
                .span_count = 0,
                .time = time
            };
        }

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

    optional<RouteInfo> FindRoute(string_view stop_from, string_view stop_to) const;
    optional<std::vector<RouteData>> BuildRoute(string_view from, string_view to) const;
    
    Graph GetGraph() const;

private:
    double bus_wait_time_{0.0};
    double bus_velocity_ {0.0};

    graph::DirectedWeightedGraph<double> graph_;
    std::unique_ptr<graph::Router<double>> router_;

    std::map<std::string, graph::VertexId> stop_ids_;
    std::vector<std::string> vertex_to_stop_name_;
    std::vector<RouteData> edge_info_; 
};

class TransportRouterBuilder {
public:
    TransportRouterBuilder() = default;

    // Теперь Build принимает всё: и каталог, и настройки
    TransportRouter Build(const TransportCatalogue& catalogue, const RoutingSettings& settings) const noexcept;

private:
    // Всё задаётся через аргументы Build
};


