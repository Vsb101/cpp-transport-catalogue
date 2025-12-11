#pragma once

#include "map_renderer.h"
#include "svg.h"
#include "transport_catalogue.h"
#include "transport_router.h"

#include <optional>

// Класс RequestHandler играет роль Фасада, упрощающего взаимодействие JSON reader-а
// с другими подсистемами приложения.
// См. паттерн проектирования Фасад: https://ru.wikipedia.org/wiki/Фасад_(шаблон_проектирования)
class RequestHandler {
public:
    RequestHandler(const TransportCatalogue& db,
                   const renderer::MapRenderer& renderer,
                   const TransportRouter& router);

    [[nodiscard]] std::optional<RouteInfo> GetBusStat(const std::string_view& bus_name) const;

    [[nodiscard]] std::set<std::string_view> GetBusesByStop(const std::string_view& stop_name) const;

    [[nodiscard]] svg::Document RenderMap() const;

    [[nodiscard]] const Stop* GetStop(const std::string_view& stop_name) const;

    [[nodiscard]] std::optional<std::vector<TransportRouter::RouteData>> BuildRoute(
        std::string_view from, std::string_view to) const;


private:
    const TransportCatalogue& db_;
    const renderer::MapRenderer& renderer_;
    const TransportRouter& router_;
};
