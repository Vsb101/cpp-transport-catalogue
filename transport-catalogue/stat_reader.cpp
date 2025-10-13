#include <iostream>
#include <string>
#include <unordered_set>
#include <iomanip>

#include "stat_reader.h"

namespace stat_reader {

namespace detail {
    
using namespace std::string_literals;

template <typename T>
std::ostream& operator<<(std::ostream& os, const std::vector<T>& vec) {
    bool first_element = true;
    for (const auto& item : vec) {
        if (!first_element) { os << " "; }
        os << item;
        first_element = false;
    }
    return os;
}

void PrintBus (const transport::TransportCatalogue& transport_catalogue,
               std::string_view request,
               std::ostream& output,
               std::string_view command) {
    const auto* bus = transport_catalogue.FindBus(command);
    if (bus == nullptr) {
        output << request << ": not found"s << std::endl;
        return;
    }
    const transport::BusInfo route = transport_catalogue.GetBusInfo(command);
    output << request << ": "s
           << route.stops_on_route << " stops on route, "s
           << route.unique_stops << " unique stops, "s

           // Форматирование длины маршрута в научной нотации
           << std::setprecision(6) << static_cast<double>(route.route_length) << " route length, "s
           << std::defaultfloat << route.curvature << " curvature\n"s;
}

void PrintStop (const transport::TransportCatalogue& transport_catalogue,
                std::string_view request,
                std::ostream& output,
                std::string_view command) { 
    const auto stop = transport_catalogue.FindStop(command);
    if (stop == nullptr) {
        output << request << ": not found\n"s;
        return;
    }
    const std::vector<std::string_view> buses = transport_catalogue.GetBusesForStop(command);
    if (buses.empty()) {
        output << request << ": no buses\n"s;
        return;
    }
    output << request << ": buses " << buses << "\n";
}

void ParseAndPrintStat(const transport::TransportCatalogue& transport_catalogue,
                       std::string_view request,
                       std::ostream& output) {
    if (request.starts_with ("Bus ")) {
        std::string_view cmd = request.substr(4);
        PrintBus(transport_catalogue, request, output, cmd);
    }
    if (request.starts_with("Stop ")) {
        std::string_view cmd = request.substr(5);
        PrintStop(transport_catalogue, request, output, cmd);
    }
}

} // namespace detail

void HandleStatRequests(const transport::TransportCatalogue& transport_catalogue, 
                        std::istream& input, 
                        std::ostream& output) {
    int stat_request_count;
    input >> stat_request_count >> std::ws;
    for (int i = 0; i < stat_request_count; ++i) {
        std::string line;
        getline(input, line);
        detail::ParseAndPrintStat(transport_catalogue, line, output);
    }
}

} // namespace stat_reader
