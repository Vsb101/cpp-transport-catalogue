#include "json_builder.h"
#include "json_reader.h"
#include "svg.h"

#include <iterator>
#include <sstream>
#include <optional>

using namespace std::literals;

namespace {

// Извлекает географические координаты (широту и долготу) из JSON-словаря.
// Валидирует наличие и тип полей. При ошибке возвращает нулевые координаты.
// Используется при добавлении остановок для построения карты маршрутов.
geo::Coordinates ExtractCoordinates(const json::Dict& dict) {
    auto lat_it = dict.find("latitude");
    auto lng_it = dict.find("longitude");
    if (lat_it == dict.end() || lng_it == dict.end() ||
        !lat_it->second.IsDouble() || !lng_it->second.IsDouble()) {
        return {0.0, 0.0};  // или бросить исключение, если нужно
    }
    return { lat_it->second.AsDouble(), lng_it->second.AsDouble() };
}

// Извлекает цвет из JSON-узла: поддерживает строковые цвета ("red", "#ff0000")
// и массивы формата [r, g, b] или [r, g, b, a]. Возвращает svg::Color.
// Если формат некорректен — возвращает пустой цвет.
svg::Color ExtractColor(const json::Node& node) {
    if (node.IsString()) { return node.AsString(); }
    if (!node.IsArray()) { return {}; }
    const auto& arr = node.AsArray();
    if (arr.size() == 3 && arr[0].IsInt() && arr[1].IsInt() && arr[2].IsInt()) {
        return svg::Rgb(
            arr[0].AsInt(),
            arr[1].AsInt(),
            arr[2].AsInt());
    }
    if (arr.size() == 4 && arr[0].IsInt() && arr[1].IsInt() && arr[2].IsInt() && arr[3].IsDouble()) {
        return svg::Rgba(
            arr[0].AsInt(),
            arr[1].AsInt(),
            arr[2].AsInt(),
            arr[3].AsDouble());
    }
    return {};
}

// Преобразует структуру RouteInfo в JSON-объект для вывода результата.
// Включает длину маршрута, кривизну и количество остановок (всего и уникальных).
json::Node AsJsonNode(const RouteInfo& info) {
    return json::Builder{}
        .StartDict()
            .Key("route_length").Value(info.length)
            .Key("curvature").Value(info.curvature)
            .Key("stop_count").Value(static_cast<int>(info.total_stops))
            .Key("unique_stop_count").Value(static_cast<int>(info.unique_stops))
        .EndDict()
        .Build();
}

}  // namespace

// Читает данные из входного потока в формате JSON.
void JsonReader::ReadData(std::istream& input) {
    document_ = json::Load(input);
}

// Основной метод инициализации транспортного справочника и карты.
// Последовательно обрабатывает базовые запросы: сначала остановки, затем расстояния и маршруты.
// Обеспечивает правильный порядок загрузки данных и передаёт управление специализированным обработчикам.
// Является центральным «проводником» на этапе построения модели.
void JsonReader::ProcessBaseRequests(TransportCatalogue& db, renderer::MapRenderer& map) const {
    const auto root = document_.GetRoot().AsDict();

    auto base_requests_it = root.find("base_requests");
    if (base_requests_it == root.end() || !base_requests_it->second.IsArray()) {
        return;
    }
    const auto& base_requests = base_requests_it->second.AsArray();

    ProcessStops(base_requests, db);
    ProcessDistances(base_requests, db);
    ProcessBuses(base_requests, db, map);
}

// Обрабатывает запросы на получение информации: статистику маршрутов, список автобусов и карту.
// Для каждого запроса формирует JSON-ответ с данными или сообщением об ошибке.
// Гарантирует наличие поля "request_id" и корректный формат вывода.
// Централизованно обрабатывает все типы запросов, обеспечивая единообразие ответов.
/*void JsonReader::ProcessStatRequests(const RequestHandler& db, std::ostream& output) const {
    json::Array responses;

    const auto root = document_.GetRoot().AsDict();

    auto stat_it = root.find("stat_requests");
    if (stat_it == root.end() || !stat_it->second.IsArray()) {
        json::Print(json::Document(std::move(responses)), output);
        return;
    }
    const auto& stat_requests = stat_it->second.AsArray();

    for (const auto& request : stat_requests) {
        json::Builder builder;
        builder.StartDict();

        // Проверяем ID
        auto id_it = request.AsDict().find("id");
        if (id_it == request.AsDict().end() || !id_it->second.IsInt()) {
            builder.Key("request_id").Value(0);
            builder.Key("error_message").Value("invalid request id");
            responses.push_back(builder.EndDict().Build());
            continue;
        }
        int id = id_it->second.AsInt();
        builder.Key("request_id").Value(id);

        // Проверяем тип
        auto type_it = request.AsDict().find("type");
        if (type_it == request.AsDict().end() || !type_it->second.IsString()) {
            builder.Key("error_message").Value("invalid type");
            responses.push_back(builder.EndDict().Build());
            continue;
        }
        std::string_view type = type_it->second.AsString();

        if (type == "Bus"sv) {
            auto name_it = request.AsDict().find("name");
            if (name_it == request.AsDict().end() || !name_it->second.IsString()) {
                builder.Key("error_message").Value("invalid bus name");
            } else {
                auto route_info = db.GetBusStat(name_it->second.AsString());
                if (route_info) {
                    auto node = AsJsonNode(*route_info);
                    for (const auto& [k, v] : node.AsDict()) {
                        builder.Key(k).Value(v.GetValue());
                    }
                } else {
                    builder.Key("error_message").Value("not found");
                }
            }
        } else if (type == "Stop"sv) {
            auto name_it = request.AsDict().find("name");
            if (name_it == request.AsDict().end() || !name_it->second.IsString()) {
                builder.Key("error_message").Value("invalid stop name");
            } else {
                if (!db.GetStop(name_it->second.AsString())) {
                    builder.Key("error_message").Value("not found");
                } else {
                    auto buses = db.GetBusesByStop(name_it->second.AsString());
                    auto arr = builder.Key("buses").StartArray();
                    for (const auto& bus : buses) {
                        arr.Value(std::string(bus));
                    }
                    arr.EndArray();
                }
            }
        } else if (type == "Map"sv) {
            std::ostringstream map_ss;
            db.RenderMap().Render(map_ss);
            builder.Key("map").Value(map_ss.str());
        } else {
            builder.Key("error_message").Value("unknown type");
        }

        responses.push_back(builder.EndDict().Build());
    }

    json::Print(json::Document(std::move(responses)), output);
}*/
void JsonReader::ProcessStatRequests(const RequestHandler& db, std::ostream& output) const {
    json::Array responses;

    const auto root = document_.GetRoot().AsDict();

    auto stat_it = root.find("stat_requests");
    if (stat_it == root.end() || !stat_it->second.IsArray()) {
        json::Print(json::Document(std::move(responses)), output);
        return;
    }
    const auto& stat_requests = stat_it->second.AsArray();

    for (const auto& request : stat_requests) {
        if (request.IsDict()) {
            json::Node response = ProcessOneStatRequest(request.AsDict(), db);
            responses.push_back(std::move(response));
        }
    }
    // Вывод ответа
    OutputResponses(std::move(responses), output);
}


// Настраивает параметры визуализации карты из JSON-документа.
// Извлекает значения ширины, высоты, цветов, шрифтов и других настроек,
// используя безопасные проверки. Если параметр отсутствует — применяется значение по умолчанию.
// Поддерживает сложные типы: массивы для цветов и координат.
void JsonReader::ProcessRenderSettings(renderer::Settings& settings) const {
    const auto root = document_.GetRoot().AsDict();

    auto rs_it = root.find("render_settings");
    if (rs_it == root.end() || !rs_it->second.IsDict()) { return; }
    const auto& render_settings = rs_it->second.AsDict();

    // Извлекает double-значение по ключу из настроек рендеринга.
    // Если ключ отсутствует или значение не является числом с плавающей точкой —
    // возвращает значение по умолчанию.
    auto get_double = [&](std::string_view key, double default_value) -> double {
        auto it = render_settings.find(std::string(key));
        if (it != render_settings.end() && it->second.IsDouble()) {
            return it->second.AsDouble();
        }
        return default_value;
    };

    // Извлекает int-значение по ключу из настроек рендеринга.
    // Если ключ отсутствует или значение не является целым числом —
    // возвращает значение по умолчанию.
    auto get_int = [&](std::string_view key, int default_value) -> int {
        auto it = render_settings.find(std::string(key));
        if (it != render_settings.end() && it->second.IsInt()) {
            return it->second.AsInt();
        }
        return default_value;
    };

    // Извлекает точку (x, y) из массива [x, y] по ключу.
    // Ожидает массив из двух чисел с плавающей точкой. Если формат неверен —
    // возвращает (0.0, 0.0).
    auto get_point = [&](std::string_view key) -> svg::Point {
        auto it = render_settings.find(std::string(key));
        if (it != render_settings.end() && it->second.IsArray()) {
            const auto& arr = it->second.AsArray();
            if (arr.size() == 2 && arr[0].IsDouble() && arr[1].IsDouble()) {
                return svg::Point{ arr[0].AsDouble(), arr[1].AsDouble() };
            }
        }
        return svg::Point{0.0, 0.0};
    };

    settings.width_                = get_double("width", 800.0);
    settings.height_               = get_double("height", 600.0);
    settings.padding_              = get_double("padding", 5.0);
    settings.line_width_           = get_double("line_width", 4.0);
    settings.stop_radius_          = get_double("stop_radius", 5.0);
    settings.bus_label_font_size_  = get_int("bus_label_font_size", 20);
    settings.bus_label_offset_     = get_point("bus_label_offset");
    settings.stop_label_font_size_ = get_int("stop_label_font_size", 15);
    settings.stop_label_offset_    = get_point("stop_label_offset");
    settings.underlayer_width_     = get_double("underlayer_width", 3.0);

    auto color_it = render_settings.find("underlayer_color");
    if (color_it != render_settings.end()) {
        settings.underlayer_color_ = ExtractColor(color_it->second);
    }

    auto palette_it = render_settings.find("color_palette");
    if (palette_it != render_settings.end() && palette_it->second.IsArray()) {
        const auto& palette = palette_it->second.AsArray();
        settings.color_palette_.clear();
        settings.color_palette_.reserve(palette.size());
        for (const auto& color_node : palette) {
            settings.color_palette_.push_back(ExtractColor(color_node));
        }
    }
}

// Обрабатывает список базовых запросов, извлекая и добавляя в справочник остановки.
// Для каждой остановки читает название и координаты, игнорируя некорректные записи.
// Является первым шагом в построении транспортной модели.
void JsonReader::ProcessStops(const json::Array& base_requests, TransportCatalogue& db) const {
    // Добавляем остановки
    for (const auto& request : base_requests) {
        if (!request.IsDict()) continue;
        const auto& dict = request.AsDict();

        auto type_it = dict.find("type");
        if (type_it == dict.end() || !type_it->second.IsString()) continue;
        std::string_view type = type_it->second.AsString();

        if (type == "Stop"sv) {
            auto name_it = dict.find("name");
            if (name_it == dict.end() || !name_it->second.IsString()) continue;
            std::string_view name = name_it->second.AsString();
            db.AddStop(std::string(name), ExtractCoordinates(dict));
        }
    }
}

// Загружает данные о расстояниях между остановками из базовых запросов.
// Поддерживает несимметричные расстояния (A → B ≠ B → A) и игнорирует невалидные поля.
// Позволяет точно рассчитывать реальную длину маршрутов.
void JsonReader::ProcessDistances(const json::Array& base_requests, TransportCatalogue& db) const {
    // Добавляем расстояния
    for (const auto& request : base_requests) {
        if (!request.IsDict()) continue;
        const auto& dict = request.AsDict();

        auto type_it = dict.find("type");
        if (type_it == dict.end() || !type_it->second.IsString()) continue;
        std::string_view type = type_it->second.AsString();

        if (type == "Stop"sv) {
            auto name_it = dict.find("name");
            if (name_it == dict.end() || !name_it->second.IsString()) continue;
            std::string stopname = std::string(name_it->second.AsString());

            auto dist_it = dict.find("road_distances");
            if (dist_it == dict.end() || !dist_it->second.IsDict()) continue;
            const auto& distances = dist_it->second.AsDict();

            for (const auto& [dst_name, node] : distances) {
                if (node.IsInt()) {
                    db.AddDistance(stopname, std::string(dst_name), node.AsInt());
                }
            }
        }
    }
}

// Разбирает описания автобусных маршрутов и добавляет их в справочник.
// Автоматически формирует кольцевые маршруты при необходимости и передаёт данные рендереру.
// Завершает этап инициализации, подготавливая данные для визуализации.
void JsonReader::ProcessBuses(const json::Array& base_requests, TransportCatalogue& db, renderer::MapRenderer& map) const {
    // Добавляем маршруты
    for (const auto& request : base_requests) {
        if (!request.IsDict()) continue;
        const auto& dict = request.AsDict();

        auto type_it = dict.find("type");
        if (type_it == dict.end() || !type_it->second.IsString()) continue;
        std::string_view type = type_it->second.AsString();

        if (type == "Bus"sv) {
            auto name_it = dict.find("name");
            if (name_it == dict.end() || !name_it->second.IsString()) continue;
            std::string busname = std::string(name_it->second.AsString());

            auto stops_it = dict.find("stops");
            if (stops_it == dict.end() || !stops_it->second.IsArray()) continue;
            const auto& stops_array = stops_it->second.AsArray();

            std::vector<std::string_view> stops;
            for (const auto& stop : stops_array) {
                if (stop.IsString()) {
                    stops.push_back(stop.AsString());
                }
            }

            bool is_roundtrip = true;
            auto roundtrip_it = dict.find("is_roundtrip");
            if (roundtrip_it != dict.end() && roundtrip_it->second.IsBool()) {
                is_roundtrip = roundtrip_it->second.AsBool();
            }

            if (!is_roundtrip) {
                std::vector<std::string_view> results(stops.begin(), stops.end());
                results.insert(results.end(), std::next(stops.rbegin()), stops.rend());
                stops = std::move(results);
            } else if (!stops.empty() && stops.front() != stops.back()) {
                stops.push_back(stops.front());
            }

            if (!stops.empty()) {
                db.AddRoute(busname, stops, is_roundtrip);
                const Bus* bus = db.FindRoute(busname);
                if (bus) {
                    map.AddBus(*bus);
                }
            }
        }
    }
}

// Обрабатывает один статистический запрос и формирует JSON-ответ.
// Этапы:
// 1 Проверяет наличие и корректность полей "id" и "type".
// 2 В зависимости от типа ("Bus", "Stop", "Map") — запрашивает данные из RequestHandler.
// 3 Формирует ответ с данными или сообщением об ошибке.
// Гарантирует, что поле "request_id" присутствует всегда.
json::Node JsonReader::ProcessOneStatRequest(const json::Dict& request, const RequestHandler& db) const {
    json::Builder builder;
    builder.StartDict();

    // Валидация ID
    auto id_it = request.find("id");
    if (id_it == request.end() || !id_it->second.IsInt()) {
        builder.Key("request_id").Value(0);
        builder.Key("error_message").Value("invalid request id");
        return builder.EndDict().Build();
    }
    int id = id_it->second.AsInt();
    builder.Key("request_id").Value(id);

    // Валидация type
    auto type_it = request.find("type");
    if (type_it == request.end() || !type_it->second.IsString()) {
        builder.Key("error_message").Value("invalid type");
        return builder.EndDict().Build();
    }
    std::string_view type = type_it->second.AsString();

    // Обработка по типу
    if (type == "Bus"sv) {
        auto name_it = request.find("name");
        if (name_it == request.end() || !name_it->second.IsString()) {
            builder.Key("error_message").Value("invalid bus name");
        } else {
            auto route_info = db.GetBusStat(name_it->second.AsString());
            if (route_info) {
                auto node = AsJsonNode(*route_info);
                for (const auto& [k, v] : node.AsDict()) {
                    builder.Key(k).Value(v.GetValue());
                }
            } else {
                builder.Key("error_message").Value("not found");
            }
        }
    } else if (type == "Stop"sv) {
        auto name_it = request.find("name");
        if (name_it == request.end() || !name_it->second.IsString()) {
            builder.Key("error_message").Value("invalid stop name");
        } else {
            if (!db.GetStop(name_it->second.AsString())) {
                builder.Key("error_message").Value("not found");
            } else {
                auto buses = db.GetBusesByStop(name_it->second.AsString());
                auto arr = builder.Key("buses").StartArray();
                for (const auto& bus : buses) {
                    arr.Value(std::string(bus));
                }
                arr.EndArray();
            }
        }
    } else if (type == "Map"sv) {
        std::ostringstream map_ss;
        db.RenderMap().Render(map_ss);
        builder.Key("map").Value(map_ss.str());
    } else {
        builder.Key("error_message").Value("unknown type");
    }

    return builder.EndDict().Build();
}

// Печатает итоговый массив ответов в выходной поток в формате JSON.
// Используется для отправки результата обработки stat_requests.
// Обёртка вокруг json::Print - вроде и не требется пока что
void JsonReader::OutputResponses(json::Array responses, std::ostream& output) const {
    json::Print(json::Document(std::move(responses)), output);
}
