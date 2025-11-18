#pragma once

#include <istream>

#include "json.h"
#include "request_handler.h"
#include "transport_catalogue.h"

// Класс JsonReader отвечает за загрузку и обработку JSON-данных:
// - чтение базовых запросов (остановки, маршруты, расстояния),
// - обработку запросов на получение информации,
// - настройку параметров рендеринга.
class JsonReader {
public:
    JsonReader() = default;

    void ReadData(std::istream &input);
    void ProcessBaseRequests(TransportCatalogue &db, renderer::MapRenderer& map) const;
    void ProcessStatRequests(const RequestHandler &db, std::ostream &output) const;
    void ProcessRenderSettings(renderer::Settings& settings) const;

private:
    void ProcessStops(const json::Array& base_requests, TransportCatalogue& db) const;
    void ProcessDistances(const json::Array& base_requests, TransportCatalogue& db) const;
    void ProcessBuses(const json::Array& base_requests, TransportCatalogue& db, renderer::MapRenderer& map) const;

    json::Node ProcessOneStatRequest(const json::Dict& request, const RequestHandler& db) const;
    void OutputResponses(json::Array responses, std::ostream& output) const;

    json::Document document_{{}};
};
