#pragma once

#include <istream>

#include "json.h"
#include "request_handler.h"
#include "transport_catalogue.h"

class JsonReader {
public:
    JsonReader() = default;

    void ReadData(std::istream &input);
    void ProcessBaseRequests(TransportCatalogue &db, renderer::MapRenderer& map) const;
    void ProcessStatRequests(const RequestHandler &db, std::ostream &output) const;
    void ProcessRenderSettings(renderer::Settings& settings) const;

private:
    json::Document document_{{}};
};
