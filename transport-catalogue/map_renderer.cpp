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

// Основной метод визуализации: формирует SVG-документ с картой.
// Этапы:
// 1. Проектирование координат.
// 2. Отрисовка линий маршрутов.
// 3. Отрисовка подписей автобусов (с подложкой).
// 4. Отрисовка кружков остановок.
// 5. Отрисовка подписей остановок (с подложкой).
// Использует цвета из палитры по циклу.
void MapRenderer::Render(svg::Document &svg_out) const {
    auto coordinates = ExtractAllCoordinates(buses_);
    const SphereProjector projector(
        coordinates.begin(),
        coordinates.end(),
        settings_.width_, settings_.height_, settings_.padding_
    );

    size_t color_number = 0;

    // Отрисовка линий маршрутов
    for (const Bus &bus : buses_) {
        svg::Polyline route_line;
        for (const Stop *stop : bus.route_) {
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

    // Отрисовка ярлыков автобусов
    color_number = 0;
    for (const Bus &bus : buses_) {
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

    //Сбор и сортировка уникальных остановок
    std::set<const Stop*, StopnameComparator> sorted_stops;
    for (const Bus& bus : buses_) {
        for (const Stop* stop : bus.route_) {
            sorted_stops.insert(stop);
        }
    }

    //Отрисовка кружков остановок
    svg::Circle stop_point = svg::Circle()
        .SetRadius(settings_.stop_radius_)
        .SetFillColor(settings_.color_palette_[color_number])
        .SetFillColor("white");

    for (const Stop* stop : sorted_stops) {
        svg_out.Add(stop_point.SetCenter(projector(stop->position_)));
    }

    // Отрисовка подписей остановок
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

} // renderer
