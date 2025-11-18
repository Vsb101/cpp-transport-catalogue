#include "request_handler.h"

// Инициализирует обработчик запросов, сохраняя ссылки на базу данных и рендерер карты.
// Используется для предоставления единого интерфейса к функционалу справочника и визуализации.
// Объекты передаются по константной ссылке — владение не передаётся.
// Гарантируется, что ссылки будут действительны на всём протяжении жизни RequestHandler.
RequestHandler::RequestHandler(const TransportCatalogue &db, const renderer::MapRenderer &renderer)
    : db_(db), renderer_(renderer) {}

// Возвращает статистику по автобусному маршруту: количество остановок, уникальных остановок, длину и кривизну.
// Если маршрут с указанным именем не найден — возвращает std::nullopt.
// Обёртка над методом TransportCatalogue::BusRouteInfo с обработкой исключений.
// Результат содержит данные, необходимые для ответа на запрос "Bus".
std::optional<RouteInfo> RequestHandler::GetBusStat(const std::string_view &bus_name) const {
    try {
        return db_.BusRouteInfo(bus_name);
    } catch (std::out_of_range& ) {
        return std::nullopt;
    }
}

// Возвращает множество названий автобусных маршрутов, проходящих через указанную остановку.
// Если остановка не найдена, возвращает пустое множество (метод TransportCatalogue::StopInfo гарантирует это поведение).
// Используется для формирования ответа на запрос "Stop".
// Результат автоматически отсортирован благодаря использованию std::set.
std::set<std::string_view> RequestHandler::GetBusesByStop(const std::string_view &stop_name) const {
    return db_.StopInfo(stop_name);
}

// Формирует SVG-документ с картой маршрутов и остановок.
// Делегирует рендеринг внутреннему объекту renderer_.
// Возвращает готовый svg::Document, который можно сразу вывести в поток.
// Этот метод будет использоваться при обработке запроса "Map".
svg::Document RequestHandler::RenderMap() const {
    svg::Document doc;
    renderer_.Render(doc);
    return doc;
}

// Возвращает указатель на остановку по её имени.
const Stop* RequestHandler::GetStop(const std::string_view& stop_name) const {
    return db_.FindStop(stop_name);
}

