#include "json_reader.h"

#include <fstream>
#include <iostream>

int main() {
    using namespace std;

    //ifstream file("test.json"s);
    //ofstream svg_image("result.svg"s);

    TransportCatalogue catalogue;
    JsonReader reader;

    reader.ReadData(cin);
    renderer::Settings settings;
    reader.ProcessRenderSettings(settings);
    renderer::MapRenderer renderer(settings);
    reader.ProcessBaseRequests(catalogue, renderer);
    RequestHandler handler(catalogue, renderer);
    reader.ProcessStatRequests(handler, cout);
    //handler.RenderMap().Render(cout);
}