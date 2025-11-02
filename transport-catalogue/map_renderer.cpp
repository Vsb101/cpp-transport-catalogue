#include "map_renderer.h"

#include <utility>
#include <vector>
#include <unordered_set>

namespace renderer {

// Конструктор инициализирует настройки рендерера.
// Принимает объект Settings по значению и сохраняет его.
// Не выполняет инициализацию данных — это делается при отрисовке.
// Гарантирует, что настройки будут доступны при вызове Render.
MapRenderer::MapRenderer(Settings settings) :
    settings_(std::move(settings)) {}

// Добавляет маршрут в список для отрисовки.
// Использует std::set с компаратором по имени, чтобы избежать дублирования.
// Маршруты будут отсортированы при отрисовке (для детерминированности).
// Не проверяет существование — дубли игнорируются автоматически.
void MapRenderer::AddBus(const Bus &bus) {
    buses_.insert(bus);
}

// Компаратор для сортировки указателей на остановки по имени.
// Используется для упорядоченного обхода остановок при отрисовке.
// Гарантирует, что подписи и кружки будут добавлены в лексикографическом порядке.
// Требуется, чтобы порядок был одинаковым при всех вызовах.
template<typename Comp>
std::vector<geo::Coordinates> ExtractAllCoordinates(const std::set<Bus, Comp> &buses) {
    std::vector<geo::Coordinates> all_coordinates;
    for (const Bus &bus: buses) {
        for (const Stop *stop: bus.route_) {
            all_coordinates.push_back(stop->position_);
        }
    }
    return all_coordinates;
}

// Универсальный шаблон для отрисовки остановок без дублирования.
// Принимает итераторы и функцию отрисовки одной остановки.
// Использует множество для отслеживания уже отрисованных остановок.
// Применяется, например, для отрисовки подписей.
// Позволяет избежать дублирования в сложных сценариях.
template<typename Iter, typename Func>
void RenderStops(Iter begin, Iter end, Func func) {
    std::unordered_set<const Stop *> rendered_stops;
    std::for_each(begin, end, [&](const Bus &bus) {
        for (const Stop *stop: std::set<const Stop *, StopnameComparator>(bus.route_.begin(), bus.route_.end())) {
            if (rendered_stops.find(stop) == rendered_stops.end()) {
                func(stop);
                rendered_stops.insert(stop);
            }
        }
    });
}

// Отрисовывает линии маршрутов автобусов на карте.
// Для каждого маршрута строится ломаная линия через все остановки.
// Использует цвета из палитры по циклу, толщину и стиль из настроек.
// Линии округлены на углах (ROUND join/cap) для визуальной плавности.
void MapRenderer::RenderBusRoutes(svg::Document& svg_out, const SphereProjector& projector) const {
    size_t color_number = 0;
    for (const Bus& bus : buses_) {
        svg::Polyline route_line;
        for (const Stop* stop : bus.route_) {
            route_line.AddPoint(projector(stop->position_));
        }

        svg_out.Add(std::move(route_line)
            .SetFillColor("none")
            .SetStrokeColor(settings_.color_palette_[color_number])
            .SetStrokeWidth(settings_.line_width_)
            .SetStrokeLineCap(svg::StrokeLineCap::ROUND)
            .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND)
        );
        color_number = (color_number + 1) % settings_.color_palette_.size();
    }
}

// Отрисовывает текстовые ярлыки названий автобусов.
// Для каждого маршрута добавляется подпись в начале и (при необходимости) в середине.
// Использует жирный шрифт и цвет из палитры, а также подложку для читаемости.
// Обеспечивает детерминированный порядок за счёт упорядочения маршрутов.
void MapRenderer::RenderBusLabels(svg::Document& svg_out, const SphereProjector& projector) const {
    size_t color_number = 0;
    for (const Bus& bus : buses_) {
        svg::Text bus_label = svg::Text()
            .SetFillColor(settings_.color_palette_[color_number])
            .SetOffset(settings_.bus_label_offset_)
            .SetFontSize(settings_.bus_label_font_size_)
            .SetFontFamily("Verdana")
            .SetFontWeight("bold");

        svg::Text bus_label_underlayer = bus_label;
        bus_label_underlayer
            .SetFillColor(settings_.underlayer_color_)
            .SetStrokeColor(settings_.underlayer_color_)
            .SetStrokeWidth(settings_.underlayer_width_)
            .SetStrokeLineCap(svg::StrokeLineCap::ROUND)
            .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);

        // Начальная остановка
        svg_out.Add(bus_label_underlayer.SetPosition(projector(bus.route_.front()->position_)).SetData(bus.name_));
        svg_out.Add(bus_label.SetPosition(projector(bus.route_.front()->position_)).SetData(bus.name_));

        // Середина маршрута (если не туда-обратно или петля)
        if (!bus.is_roundtrip_ && bus.route_.size() != 1 &&
            bus.route_.front()->position_ != bus.route_[bus.route_.size() / 2]->position_) {
            svg_out.Add(bus_label_underlayer
                .SetPosition(projector(bus.route_[bus.route_.size() / 2]->position_))
                .SetData(bus.name_));
            svg_out.Add(bus_label
                .SetPosition(projector(bus.route_[bus.route_.size() / 2]->position_))
                .SetData(bus.name_));
        }
        color_number = (color_number + 1) % settings_.color_palette_.size();
    }
}

// Отрисовывает кружки, обозначающие остановки на карте.
// Все остановки собираются в упорядоченное множество по имени.
// Каждая остановка отмечается белым кружком с заданным радиусом.
// Порядок отрисовки — лексикографический, для стабильности изображения.
void MapRenderer::RenderStopPoints(svg::Document& svg_out, const SphereProjector& projector) const {
    // Сбор и сортировка уникальных остановок
    std::set<const Stop*, StopnameComparator> sorted_stops;
    for (const Bus& bus : buses_) {
        for (const Stop* stop : bus.route_) {
            sorted_stops.insert(stop);
        }
    }

    svg::Circle stop_point = svg::Circle()
        .SetRadius(settings_.stop_radius_)
        .SetFillColor("white");

    for (const Stop* stop : sorted_stops) {
        svg_out.Add(stop_point.SetCenter(projector(stop->position_)));
    }
}

// Отрисовывает текстовые подписи названий остановок.
// Подписи добавляются поверх кружков, смещены относительно центра.
// Используется чёрный цвет текста и прозрачная подложка для контраста.
// Остановки обрабатываются в лексикографическом порядке для детерминированности.
void MapRenderer::RenderStopLabels(svg::Document& svg_out, const SphereProjector& projector) const {
    // Сбор и сортировка уникальных остановок
    std::set<const Stop*, StopnameComparator> sorted_stops;
    for (const Bus& bus : buses_) {
        for (const Stop* stop : bus.route_) {
            sorted_stops.insert(stop);
        }
    }

    svg::Text stop_label = svg::Text()
        .SetFontSize(settings_.stop_label_font_size_)
        .SetOffset(settings_.stop_label_offset_)
        .SetFontFamily("Verdana")
        .SetFillColor("black");

    svg::Text stop_label_underlayer = stop_label;
    stop_label_underlayer
        .SetFillColor(settings_.underlayer_color_)
        .SetStrokeColor(settings_.underlayer_color_)
        .SetStrokeWidth(settings_.underlayer_width_)
        .SetStrokeLineCap(svg::StrokeLineCap::ROUND)
        .SetStrokeLineJoin(svg::StrokeLineJoin::ROUND);

    for (const Stop* stop : sorted_stops) {
        svg_out.Add(stop_label_underlayer.SetPosition(projector(stop->position_)).SetData(stop->name_));
        svg_out.Add(stop_label.SetPosition(projector(stop->position_)).SetData(stop->name_));
    }
}

// Основной метод визуализации: формирует SVG-документ с картой.
// Этапы:
// 1. Проектирование координат.
// 2. Отрисовка линий маршрутов.
// 3. Отрисовка подписей автобусов (с подложкой).
// 4. Отрисовка кружков остановок.
// 5. Отрисовка подписей остановок (с подложкой).
// Использует цвета из палитры по циклу.
void MapRenderer::Render(svg::Document& svg_out) const {
    auto coordinates = ExtractAllCoordinates(buses_);
    const SphereProjector projector(
        coordinates.begin(),
        coordinates.end(),
        settings_.width_, settings_.height_, settings_.padding_
    );

    RenderBusRoutes(svg_out, projector);
    RenderBusLabels(svg_out, projector);
    RenderStopPoints(svg_out, projector);
    RenderStopLabels(svg_out, projector);
}

} // renderer
