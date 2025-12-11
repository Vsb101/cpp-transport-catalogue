#include "json_builder.h"
#include "json_reader.h"
#include "svg.h"

#include <iterator>
#include <sstream>
#include <optional>

using namespace std::literals;

namespace {

// === Ключи JSON ===
static constexpr std::string_view k_type_key = "type";
static constexpr std::string_view k_id_key = "id";
static constexpr std::string_view k_name_key = "name";
static constexpr std::string_view k_stops_key = "stops";
static constexpr std::string_view k_is_roundtrip_key = "is_roundtrip";
static constexpr std::string_view k_road_distances_key = "road_distances";
static constexpr std::string_view k_base_requests_key = "base_requests";
static constexpr std::string_view k_stat_requests_key = "stat_requests";
static constexpr std::string_view k_render_settings_key = "render_settings";
static constexpr std::string_view k_latitude_key = "latitude";
static constexpr std::string_view k_longitude_key = "longitude";
static constexpr std::string_view k_routing_settings_key = "routing_settings";
static constexpr std::string_view k_bus_wait_time_key = "bus_wait_time";
static constexpr std::string_view k_bus_velocity_key = "bus_velocity";

// === Типы запросов ===
static constexpr std::string_view k_stop_type = "Stop";
static constexpr std::string_view k_bus_type = "Bus";
static constexpr std::string_view k_map_type = "Map";
static constexpr std::string_view k_route_type = "Route";

// === Ключи рендер-настроек ===
static constexpr std::string_view k_width_key = "width";
static constexpr std::string_view k_height_key = "height";
static constexpr std::string_view k_padding_key = "padding";
static constexpr std::string_view k_line_width_key = "line_width";
static constexpr std::string_view k_stop_radius_key = "stop_radius";
static constexpr std::string_view k_bus_label_font_size_key = "bus_label_font_size";
static constexpr std::string_view k_bus_label_offset_key = "bus_label_offset";
static constexpr std::string_view k_stop_label_font_size_key = "stop_label_font_size";
static constexpr std::string_view k_stop_label_offset_key = "stop_label_offset";
static constexpr std::string_view k_underlayer_width_key = "underlayer_width";
static constexpr std::string_view k_underlayer_color_key = "underlayer_color";
static constexpr std::string_view k_color_palette_key = "color_palette";

// Извлекает географические координаты (широту и долготу) из JSON-словаря.
// Валидирует наличие и тип полей. При ошибке возвращает нулевые координаты.
// Используется при добавлении остановок для построения карты маршрутов.
std::optional<geo::Coordinates> ExtractCoordinates(const json::Dict& dict) {
    auto lat_it = dict.find(k_latitude_key);
    auto lng_it = dict.find(k_longitude_key);

    if (lat_it == dict.end() || !lat_it->second.IsDouble()) return std::nullopt;
    if (lng_it == dict.end() || !lng_it->second.IsDouble()) return std::nullopt;

    return std::make_optional<geo::Coordinates>(
        lat_it->second.AsDouble(), 
        lng_it->second.AsDouble()
    );
}

// Извлекает цвет из JSON-узла: поддерживает строковые цвета ("red", "#ff0000")
// и массивы формата [r, g, b] или [r, g, b, a]. Возвращает svg::Color.
// Если формат некорректен — возвращает пустой цвет.
svg::Color ExtractColor(const json::Node& node) {
    if (node.IsString()) { return node.AsString(); }
    if (!node.IsArray()) { return {}; }

    const auto& arr = node.AsArray();
    
    if (arr.size() == 3 && arr[0].IsInt() && arr[1].IsInt() && arr[2].IsInt()) {
        auto r = arr[0].AsInt();
        auto g = arr[1].AsInt();
        auto b = arr[2].AsInt();
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) { return {}; }

        return svg::Rgb(r, g, b);
    }

    if (arr.size() == 4 &&
        arr[0].IsInt() && arr[1].IsInt() && arr[2].IsInt() && arr[3].IsDouble()) {
        
        auto r = arr[0].AsInt();
        auto g = arr[1].AsInt();
        auto b = arr[2].AsInt();
        auto a = arr[3].AsDouble();
        if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255 || a < 0.0 || a > 1.0) { return {}; }

        return svg::Rgba(r, g, b, a);
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

// Обрабатывает базовые запросы: сначала остановки, затем расстояния, потом автобусы.
// Порядок важен: сначала должны быть добавлены остановки, чтобы для них задать расстояния.
void JsonReader::ProcessBaseRequests(TransportCatalogue& db, renderer::MapRenderer& map) const {
    const auto& root = document_.GetRoot().AsDict();

    auto base_requests_it = root.find(k_base_requests_key);
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
void JsonReader::ProcessStatRequests(const RequestHandler& db, std::ostream& output) const {
    json::Array responses;

    const auto& root = document_.GetRoot().AsDict();

    auto stat_it = root.find(k_stat_requests_key);
    if (stat_it == root.end() || !stat_it->second.IsArray()) {
        OutputResponses(std::move(responses), output);
        return;
    }

    const auto& stat_requests = stat_it->second.AsArray();
    responses.reserve(stat_requests.size());

    for (const auto& request : stat_requests) {
        if (request.IsDict()) {
            responses.push_back(ProcessOneStatRequest(request.AsDict(), db));
        }
    }

    OutputResponses(std::move(responses), output);
}

// Настраивает параметры визуализации карты из JSON-документа.
// Извлекает значения ширины, высоты, цветов, шрифтов и других настроек,
// используя безопасные проверки. Если параметр отсутствует — применяется значение по умолчанию.
// Поддерживает сложные типы: массивы для цветов и координат.
void JsonReader::ProcessRenderSettings(renderer::Settings& settings) const {
    const auto& root = document_.GetRoot().AsDict();

    auto rs_it = root.find(k_render_settings_key);
    if (rs_it == root.end() || !rs_it->second.IsDict()) { return; }
    const auto& render_settings = rs_it->second.AsDict();

    auto get_double = [&](std::string_view key, double default_value) -> double {
        auto it = render_settings.find(key);
        return (it != render_settings.end() && it->second.IsDouble()) ? it->second.AsDouble() : default_value;
    };

    auto get_int = [&](std::string_view key, int default_value) -> int {
        auto it = render_settings.find(key);
        return (it != render_settings.end() && it->second.IsInt()) ? it->second.AsInt() : default_value;
    };

    auto get_point = [&](std::string_view key) -> svg::Point {
        auto it = render_settings.find(key);
        if (it == render_settings.end() || !it->second.IsArray()) return svg::Point{0.0, 0.0};
        const auto& arr = it->second.AsArray();
        if (arr.size() != 2 || !arr[0].IsDouble() || !arr[1].IsDouble()) return svg::Point{0.0, 0.0};
        return svg::Point{ arr[0].AsDouble(), arr[1].AsDouble() };
    };

    settings.width_                = get_double(k_width_key, 800.0);
    settings.height_               = get_double(k_height_key, 600.0);
    settings.padding_              = get_double(k_padding_key, 5.0);
    settings.line_width_           = get_double(k_line_width_key, 4.0);
    settings.stop_radius_          = get_double(k_stop_radius_key, 5.0);
    settings.bus_label_font_size_  = get_int(k_bus_label_font_size_key, 20);
    settings.bus_label_offset_     = get_point(k_bus_label_offset_key);
    settings.stop_label_font_size_ = get_int(k_stop_label_font_size_key, 15);
    settings.stop_label_offset_    = get_point(k_stop_label_offset_key);
    settings.underlayer_width_     = get_double(k_underlayer_width_key, 3.0);

    auto color_it = render_settings.find(k_underlayer_color_key);
    if (color_it != render_settings.end()) {
        settings.underlayer_color_ = ExtractColor(color_it->second);
    }

    auto palette_it = render_settings.find(k_color_palette_key);
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
    for (const auto& request : base_requests) {
        if (!request.IsDict()) continue;
        const auto& dict = request.AsDict();

        auto stop_name = GetRequestNameIfType(dict, k_stop_type);
        if (!stop_name) continue;

        auto coords = ExtractCoordinates(dict);
        if (!coords) continue;
        db.AddStop(std::string(*stop_name), *coords);
    }
}


// Загружает данные о расстояниях между остановками из базовых запросов.
// Поддерживает несимметричные расстояния (A → B ≠ B → A) и игнорирует невалидные поля.
// Позволяет точно рассчитывать реальную длину маршрутов.
void JsonReader::ProcessDistances(const json::Array& base_requests, TransportCatalogue& db) const {
    for (const auto& request : base_requests) {
        if (!request.IsDict()) continue;
        const auto& dict = request.AsDict();

        // Проверяем, что это Stop, и получаем имя
        auto stopname = GetRequestNameIfType(dict, k_stop_type);
        if (!stopname) continue;

        auto dist_it = dict.find(k_road_distances_key);
        if (dist_it == dict.end() || !dist_it->second.IsDict()) continue;
        const auto& distances = dist_it->second.AsDict();

        for (const auto& [dst_name, node] : distances) {
            if (node.IsInt()) {
                db.AddDistance(std::string(*stopname), std::string(dst_name), node.AsInt());
            }
        }
    }
}

// Разбирает описания автобусных маршрутов и добавляет их в справочник.
// Автоматически формирует кольцевые маршруты при необходимости и передаёт данные рендереру.
// Завершает этап инициализации, подготавливая данные для визуализации.
void JsonReader::ProcessBuses(const json::Array& base_requests, TransportCatalogue& db, renderer::MapRenderer& map) const {
    for (const auto& request : base_requests) {
        if (!request.IsDict()) continue;
        const auto& dict = request.AsDict();

        // Проверяем, что это Bus-запрос, и извлекаем имя
        auto bus_name = GetRequestNameIfType(dict, k_bus_type);
        if (!bus_name) continue;

        // Извлечение остановок
        auto stops_it = dict.find(k_stops_key);
        if (stops_it == dict.end() || !stops_it->second.IsArray()) continue;
        const auto& stops_array = stops_it->second.AsArray();

        std::vector<std::string_view> stops;
        stops.reserve(stops_array.size());
        for (const auto& stop : stops_array) {
            if (stop.IsString()) {
                stops.push_back(stop.AsString());
            }
        }

        // Проверка is_roundtrip
        bool is_roundtrip = true;
        auto roundtrip_it = dict.find(k_is_roundtrip_key);
        if (roundtrip_it != dict.end() && roundtrip_it->second.IsBool()) {
            is_roundtrip = roundtrip_it->second.AsBool();
        }

        // Построение полного маршрута
        if (!is_roundtrip) {
            // Туда и обратно: A B C → A B C B A (без дублирования последней)
            std::vector<std::string_view> result;
            result.reserve(stops.size() * 2 - 1);
            result.assign(stops.begin(), stops.end());
            result.insert(result.end(), std::next(stops.rbegin()), stops.rend());
            stops = std::move(result);
        } 
        else if (!stops.empty() && stops.front() != stops.back()) {
            // Кольцевой маршрут: добавляем первую остановку в конец
            stops.push_back(stops.front());
        }

        // Добавляем в каталог, если есть остановки
        if (!stops.empty()) {
            db.AddRoute(std::string(*bus_name), stops, is_roundtrip);
            if (const Bus* bus = db.FindRoute(*bus_name)) {
                map.AddBus(*bus);
            }
        }
    }
}

// Обрабатывает один статистический запрос и формирует JSON-ответ.
// Этапы:
// 1 Проверяет наличие и корректность полей "id" и "type".
// 2 В зависимости от типа ("Bus", "Stop", "Map", "Route") — запрашивает данные из RequestHandler.
// 3 Формирует ответ с данными или сообщением об ошибке.
// Гарантирует, что поле "request_id" присутствует всегда.
json::Node JsonReader::ProcessOneStatRequest(const json::Dict& request, const RequestHandler& db) const {
    json::Builder builder;
    builder.StartDict();

    // Извлекаем request_id
    auto id_it = request.find(k_id_key);
    if (id_it == request.end() || !id_it->second.IsInt()) {
        return builder.Key("request_id").Value(0)
                        .Key("error_message").Value("invalid request id")
                        .EndDict()
                        .Build();
    }
    int id = id_it->second.AsInt();
    builder.Key("request_id").Value(id);

    // Извлекаем тип запроса
    auto type_it = request.find(k_type_key);
    if (type_it == request.end() || !type_it->second.IsString()) {
        return builder.Key("error_message").Value("invalid type")
                        .EndDict()
                        .Build();
    }
    std::string_view type = type_it->second.AsString();

    // Обработка по типу
    if (type == k_bus_type) {
        auto name_it = request.find(k_name_key);
        if (name_it == request.end() || !name_it->second.IsString()) {
            return builder.Key("error_message").Value("invalid bus name")
                            .EndDict()
                            .Build();
        }
        std::string_view bus_name = name_it->second.AsString();

        auto route_info = db.GetBusStat(bus_name);
        if (!route_info) {
            return builder.Key("error_message").Value("not found")
                            .EndDict()
                            .Build();
        }

    
        auto node = AsJsonNode(*route_info);
        const auto& dict = node.AsDict();
        for (const auto& [key, value] : dict) {
            builder.Key(key).Value(value.GetValue());
        }
        return builder.EndDict().Build();

    } else if (type == k_stop_type) {
        auto name_it = request.find(k_name_key);
        if (name_it == request.end() || !name_it->second.IsString()) {
            return builder.Key("error_message").Value("invalid stop name")
                            .EndDict()
                            .Build();
        }
        std::string_view stop_name = name_it->second.AsString();
        if (!db.GetStop(stop_name)) {
            return builder.Key("error_message").Value("not found")
                            .EndDict()
                            .Build();
        }
        auto buses = db.GetBusesByStop(stop_name);
        builder.Key("buses").StartArray();
        for (const auto& bus : buses) {
            builder.Value(std::string(bus));
        }
        builder.EndArray();
        return builder.EndDict().Build();

    } else if (type == k_map_type) {
        std::ostringstream map_ss;
        db.RenderMap().Render(map_ss);
        return builder.Key("map").Value(map_ss.str())
                        .EndDict()
                        .Build();

    } else if (type == k_route_type) {
        auto from_it = request.find("from");
        auto to_it = request.find("to");
        if (from_it == request.end() || to_it == request.end() ||
            !from_it->second.IsString() || !to_it->second.IsString()) {
            return builder.Key("error_message").Value("invalid route request")
                            .EndDict()
                            .Build();
        }

        std::string_view from = from_it->second.AsString();
        std::string_view to = to_it->second.AsString();

        auto route_data = db.BuildRoute(from, to);
        if (!route_data) {
            return builder.Key("error_message").Value("not found")
                            .EndDict()
                            .Build();
        }

        double total_time = 0.0;
        builder.Key("items").StartArray();
        for (const auto& item : *route_data) {
            total_time += item.time;
            builder.StartDict();
            builder.Key("type").Value(item.type);
            if (item.type == "Wait") {
                builder.Key("stop_name").Value(item.stop_name);
            } else if (item.type == "Bus") {
                builder.Key("bus").Value(item.bus_name);
                builder.Key("span_count").Value(static_cast<int>(item.span_count));
            }
            builder.Key("time").Value(item.time);
            builder.EndDict();
        }
        builder.EndArray();

        builder.Key("total_time").Value(total_time);
        return builder.EndDict().Build();

    } else {
        return builder.Key("error_message").Value("unknown type")
                        .EndDict()
                        .Build();
    }
}


// Печатает итоговый массив ответов в выходной поток в формате JSON.
// Используется для отправки результата обработки stat_requests.
void JsonReader::OutputResponses(json::Array responses, std::ostream& output) const {
    json::Print(json::Document(std::move(responses)), output);
}

// Проверяет, является ли JSON-объект запросом указанного типа (например, "Stop" или "Bus"),
// и возвращает имя объекта, если проверка пройдена.
// Возвращает std::nullopt, если:
// - отсутствует поле "type",
// - "type" не является строкой,
// - значение "type" не совпадает с ожидаемым,
// - отсутствует поле "name" или оно не строка.
// Используется для единообразной обработки типизированных запросов в ProcessStops, ProcessBuses и других методах.
std::optional<std::string_view> JsonReader::GetRequestNameIfType(const json::Dict& dict, std::string_view expected_type) {
    auto type_it = dict.find(k_type_key);
    if (type_it == dict.end() || !type_it->second.IsString()) {
        return std::nullopt;
    }
    if (type_it->second.AsString() != expected_type) {
        return std::nullopt;
    }

    auto name_it = dict.find(k_name_key);
    if (name_it == dict.end() || !name_it->second.IsString()) {
        return std::nullopt;
    }
    return name_it->second.AsString();
}

// Извлекает настройки маршрутизации: время ожидания и скорость автобуса.
// Ожидает поле "routing_settings" с двумя полями: bus_wait_time (сек/мин) и bus_velocity (км/ч).
// Проверяет типы: bus_wait_time — число, bus_velocity — число.
// При ошибке выбрасывает invalid_argument.
// Возвращает объект RoutingSettings, используемый для создания TransportRouter.
RoutingSettings JsonReader::ReadRoutingSettings() const {
    const auto& root = document_.GetRoot().AsDict();

    auto it = root.find(k_routing_settings_key);
    if (it == root.end() || !it->second.IsDict()) {
        throw std::invalid_argument("Routing settings not found in JSON");
    }

    const auto& settings = it->second.AsDict();

    auto wait_it = settings.find(k_bus_wait_time_key);
    auto vel_it  = settings.find(k_bus_velocity_key);

    if (wait_it == settings.end() || !wait_it->second.IsInt() ||
        vel_it == settings.end() || !vel_it->second.IsDouble()) {
        throw std::invalid_argument("Invalid routing settings");
    }

    return RoutingSettings{
        .bus_wait_time = wait_it->second.AsDouble(),
        .bus_velocity = vel_it->second.AsDouble()
    };
}

