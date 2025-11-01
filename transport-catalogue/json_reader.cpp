#include "json_reader.h"
#include "svg.h"

#include <algorithm>
#include <sstream>

// Загружает JSON-документ из входного потока и сохраняет его во внутреннее состояние.
// Используется для последующей обработки базовых и статистических запросов.
// Вызывается в начале обработки входных данных.
// Ожидает корректный JSON на входе.
// Может выбросить исключение при ошибке парсинга.
void JsonReader::ReadData(std::istream &input) {
    document_ = json::Load(input);
}

// Извлекает координаты (широту и долготу) из JSON-объекта, содержащего поля "latitude" и "longitude".
// Используется при добавлении остановок в транспортный справочник.
// Предполагает, что оба поля присутствуют и имеют числовой тип.
// Возвращает объект geo::Coordinates.
// Выбрасывает исключение, если поля отсутствуют или неверного типа.
geo::Coordinates ExtractCoordinates(const json::Dict &dict) {
    return {dict.at("latitude").AsDouble(), dict.at("longitude").AsDouble()};
}

// Обрабатывает базовые запросы: добавляет остановки, расстояния между ними и маршруты.
// Сначала добавляет все остановки, затем — расстояния, затем — маршруты.
// Для каждого маршрута определяет, круговой ли он, и формирует список остановок.
// Добавляет маршрут в базу и регистрирует его в рендерере карты.
// Ожидает, что входные данные содержат массив "base_requests".
void JsonReader::ProcessBaseRequests(TransportCatalogue &db, renderer::MapRenderer &map) const {
    using namespace std::literals;
    const auto base_requests = document_.GetRoot().AsMap().at("base_requests"s).AsArray();

    for (const auto &request: base_requests) {
        if (request.AsMap().at("type"s) == "Stop"s) {
            db.AddStop(request.AsMap().at("name"s).AsString(), ExtractCoordinates(request.AsMap()));
        }
    }

    for (const auto &request: base_requests) {
        if (request.AsMap().at("type"s) == "Stop"s) {
            std::string stopname = request.AsMap().at("name"s).AsString();
            for (const auto &[name, distance]: request.AsMap().at("road_distances"s).AsMap()) {
                db.AddDistance(stopname, name, distance.AsInt());
            }
        }
    }

    for (const auto &request: base_requests) {
        if (request.AsMap().at("type"s) == "Bus"s) {
            std::string busname = request.AsMap().at("name"s).AsString();
            std::vector<std::string_view> stops;
            for (const auto &stop: request.AsMap().at("stops"s).AsArray()) {
                stops.push_back(stop.AsString());
            }
            if (!request.AsMap().at("is_roundtrip"s).AsBool()) {
                std::vector<std::string_view> results(stops.begin(), stops.end());
                results.insert(results.end(), std::next(stops.rbegin()), stops.rend());
                stops = std::move(results);
            } else if (stops.front() != stops.back()) {
                stops.push_back(stops.front());
            }
            db.AddRoute(busname, stops, request.AsMap().at("is_roundtrip"s).AsBool());
            map.AddBus(db.FindRoute(busname));
        }
    }
}

// Преобразует информацию о маршруте (RouteInfo) в JSON-объект (json::Node).
// Включает длину маршрута, кривизну, количество остановок и уникальных остановок.
// Используется при формировании ответов на запросы типа "Bus".
// Возвращает json::Node, содержащий словарь с данными.
// Не выбрасывает исключения при корректном входе.
json::Node AsJsonNode(const RouteInfo &route_info) {
    using namespace std::literals;
    json::Dict dict;
    dict["route_length"s] = route_info.length;
    dict["curvature"s] = route_info.curvature;
    dict["stop_count"s] = static_cast<int>(route_info.total_stops);
    dict["unique_stop_count"s] = static_cast<int>(route_info.unique_stops);
    return dict;
}

// Обрабатывает запросы: Bus, Stop, Map.
// Для каждого запроса формирует JSON-ответ с идентификатором и данными.
// При запросе "Bus" — возвращает статистику по маршруту.
// При запросе "Stop" — список автобусов, проходящих через остановку.
// При запросе "Map" — SVG-представление карты в виде строки.
void JsonReader::ProcessStatRequests(const RequestHandler &db, std::ostream &output) const {
    using namespace std::literals;
    const auto stat_requests = document_.GetRoot().AsMap().at("stat_requests"s).AsArray();

    json::Array responses;

    for (const auto &request : stat_requests) {
        json::Dict response;
        response["request_id"s] = request.AsMap().at("id"s).AsInt();

        const std::string_view type = request.AsMap().at("type"s).AsString();

        if (type == "Bus"sv) {
            auto route_info = db.GetBusStat(request.AsMap().at("name"s).AsString());
            if (route_info.has_value()) {
                auto info = AsJsonNode(*route_info).AsMap();
                response.insert(info.begin(), info.end());
            } else {
                response["error_message"s] = "not found"s;
            }
        } 
        else if (type == "Stop"sv) {
            try {
                json::Array buses;
                for (std::string_view bus : db.GetBusesByStop(request.AsMap().at("name"s).AsString())) {
                    buses.emplace_back(std::string(bus));
                }
                response["buses"s] = std::move(buses);
            } catch (std::out_of_range&) {
                response["error_message"s] = "not found"s;
            }
        }
        else if (type == "Map"sv) {
            svg::Document map = db.RenderMap();
            std::ostringstream map_output;
            map.Render(map_output);
            response["map"] = map_output.str();
        }
        else {
            response["error_message"s] = "unknown type"s;
        }
        responses.emplace_back(std::move(response));
    }
    json::Print(json::Document(std::move(responses)), output);
}

// Преобразует JSON-узел в объект цвета SVG (svg::Color).
// Поддерживает строковые значения ("red", "#FF0000") и массивы [r,g,b] или [r,g,b,a].
// Возвращает svg::Rgb для 3-компонентного массива, svg::Rgba — для 4-компонентного.
// Возвращает пустой цвет, если формат не распознан.
// Используется при чтении настроек рендеринга карты.
svg::Color ExtractColor(const json::Node &node) {
    if (node.IsString()) {
        return node.AsString();
    }
    if (node.IsArray()) {
        if (node.AsArray().size() == 3) {
            return svg::Rgb(node.AsArray()[0].AsInt(), node.AsArray()[1].AsInt(), node.AsArray()[2].AsInt());
        } else if (node.AsArray().size() == 4) {
            return svg::Rgba(node.AsArray()[0].AsInt(), node.AsArray()[1].AsInt(), node.AsArray()[2].AsInt(),
                             node.AsArray()[3].AsDouble());
        }
    }
    return {};
}

// Извлекает настройки рендеринга карты из JSON и заполняет объект Settings.
// Читает ширину, высоту, отступы, стили линий, шрифтов, цвета и палитру.
// Цвета обрабатываются с помощью вспомогательной функции ExtractColor.
// Цвет фона и палитра цветов линий автобусов также устанавливаются.
// Вызывается перед рендерингом карты.
void JsonReader::ProcessRenderSettings(renderer::Settings &settings) const {
    using namespace std::literals;
    const auto render_settings = document_.GetRoot().AsMap().at("render_settings"s).AsMap();

    settings.width_ = render_settings.at("width"s).AsDouble();
    settings.height_ = render_settings.at("height"s).AsDouble();
    settings.padding_ = render_settings.at("padding"s).AsDouble();
    settings.line_width_ = render_settings.at("line_width"s).AsDouble();
    settings.stop_radius_ = render_settings.at("stop_radius"s).AsDouble();
    settings.bus_label_font_size_ = render_settings.at("bus_label_font_size"s).AsInt();
    settings.bus_label_offset_ = {render_settings.at("bus_label_offset"s).AsArray()[0].AsDouble(),
                                  render_settings.at("bus_label_offset"s).AsArray()[1].AsDouble()};
    settings.stop_label_font_size_ = render_settings.at("stop_label_font_size"s).AsInt();
    settings.stop_label_offset_ = {render_settings.at("stop_label_offset"s).AsArray()[0].AsDouble(),
                                   render_settings.at("stop_label_offset"s).AsArray()[1].AsDouble()};
    settings.underlayer_color_ = ExtractColor(render_settings.at("underlayer_color"s));
    settings.underlayer_width_ = render_settings.at("underlayer_width"s).AsDouble();
    settings.color_palette_.reserve(render_settings.at("color_palette"s).AsArray().size());

    for (const auto &color: render_settings.at("color_palette"s).AsArray()) {
        settings.color_palette_.push_back(ExtractColor(color));
    }
}
