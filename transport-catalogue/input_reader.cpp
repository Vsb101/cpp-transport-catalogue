#include <algorithm>
#include <cassert>
#include <iterator>
#include <charconv>

#include "input_reader.h"

namespace input {

namespace detail {

//Парсит строку вида "10.123,  -30.1837" и возвращает пару координат (широта, долгота)
geo::Coordinates ParseCoordinates(std::string_view str) {
    static const double nan = std::nan("");

    auto not_space = str.find_first_not_of(' ');
    auto comma = str.find(',');

    if (comma == str.npos) {
        return {nan, nan};
    }

    auto not_space2 = str.find_first_not_of(' ', comma + 1);

    double lat = std::stod(std::string(str.substr(not_space, comma - not_space)));
    double lng = std::stod(std::string(str.substr(not_space2)));

    return {lat, lng};
}


//Удаляет пробелы в начале и конце строки
std::string_view Trim(std::string_view string) {
    const auto start = string.find_first_not_of(' ');
    if (start == string.npos) {
        return {};
    }
    return string.substr(start, string.find_last_not_of(' ') + 1 - start);
}

// Разбивает строку string на n строк, с помощью указанного символа-разделителя delim
std::vector<std::string_view> Split(std::string_view string, char delim) {
    std::vector<std::string_view> result;

    size_t pos = 0;
    while ((pos = string.find_first_not_of(' ', pos)) < string.length()) {
        auto delim_pos = string.find(delim, pos);
        if (delim_pos == string.npos) {
            delim_pos = string.size();
        }
        if (auto substr = Trim(string.substr(pos, delim_pos - pos)); !substr.empty()) {
            result.push_back(substr);
        }
        pos = delim_pos + 1;
    }

    return result;
}

/**
 * Парсит маршрут.
 * Для кольцевого маршрута (A>B>C>A) возвращает массив названий остановок [A,B,C,A]
 * Для некольцевого маршрута (A-B-C-D) возвращает массив названий остановок [A,B,C,D,C,B,A]
 */
std::vector<std::string_view> ParseRoute(std::string_view route) {
    if (route.find('>') != route.npos) {
        return Split(route, '>');
    }

    auto stops = Split(route, '-');
    std::vector<std::string_view> results(stops.begin(), stops.end());
    results.insert(results.end(), std::next(stops.rbegin()), stops.rend());

    return results;
}

CommandDescription ParseCommandDescription(std::string_view line) {
    auto colon_pos = line.find(':');
    if (colon_pos == line.npos) {
        return {};
    }

    auto space_pos = line.find(' ');
    if (space_pos >= colon_pos) {
        return {};
    }

    auto not_space = line.find_first_not_of(' ', space_pos);
    if (not_space >= colon_pos) {
        return {};
    }

    return {std::string(line.substr(0, space_pos)),
            std::string(line.substr(not_space, colon_pos - not_space)),
            std::string(line.substr(colon_pos + 1))};
}

std::unordered_map<std::string_view, int> ParseDistance(std::string_view line) {
    std::unordered_map<std::string_view, int> length_to_stops{};
    auto comma1 = line.find(',');
    auto comma2 = line.find(',', comma1 + 1);
    if (comma2 == line.npos) { return length_to_stops; }

    auto lengths = Split(line.substr(comma2 + 1), ',');
    length_to_stops.reserve(lengths.size());

    for (auto length : lengths) {
        auto m_pos = length.find("m to ");
        if (m_pos == std::string_view::npos) continue;

        std::string_view distance_part = length.substr(0, m_pos);
        std::string_view stop_name =     length.substr(m_pos + 5);

        int distance{};
        std::from_chars(distance_part.data(),
            distance_part.data() + distance_part.size(),
            distance);
        
        length_to_stops.emplace(stop_name, distance);
    }
    return length_to_stops;
}

} // namespace detail

void InputReader::ParseLine(std::string_view line) {
    auto command_description = detail::ParseCommandDescription(line);
    if (command_description) {
        commands_.push_back(std::move(command_description));
    }
}

// Делим команды на две группы: сперва идут "Stop", затем "Bus"
// Проходим списку команд и выполняем операции AddStop и AddBus
void InputReader::ApplyCommands(transport::TransportCatalogue& catalogue) {
    using namespace detail;
    using std::string_view;

    auto boundary = std::partition(
        commands_.begin(),
        commands_.end(),
        [](const CommandDescription& cmd) { return cmd.command == "Stop"; });

    for (auto it = commands_.begin(); it != boundary; ++it) {
        catalogue.AddStop(
            it->id,
            ParseCoordinates(string_view(it->description)) );
    }
    for (auto it = commands_.begin(); it != boundary; ++it) {
        auto distances = ParseDistance(string_view(it->description));
        for (const auto& [to_stop, distance] : distances) {
            catalogue.AddDistance(it->id, to_stop, distance);
        }
    }
    for (auto it = boundary; it != commands_.end(); ++it) {
        catalogue.AddBus(
            it->id,
            ParseRoute(string_view(it->description)) );
    }
}

// Для чтения базовых запросов и их обработки
void InputReader::HandleBaseRequests(std::istream& input_stream) {
    int base_request_count;
    input_stream >> base_request_count >> std::ws;
        
    for (int i = 0; i < base_request_count; ++i) {
        std::string line;
        std::getline(input_stream, line);
        ParseLine(line);
    }
}

} //namespace input